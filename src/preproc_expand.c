#define _POSIX_C_SOURCE 200809L
/*
 * Macro table management and expansion logic.
 *
 * This module stores macro definitions and performs textual replacement
 * during preprocessing.  Macros may be object-like or take parameters,
 * supporting the `#` stringize and `##` token pasting operators.  Macro
 * bodies are expanded recursively so definitions can reference other macros.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "preproc_macros.h"
#include "preproc_cond.h"
#include <stdio.h>
#include <errno.h>
#include "util.h"
#include "vector.h"
#include "strbuf.h"
#include "preproc_args.h"
#include "preproc_builtin.h"
#include "preproc_macro_utils.h"
#include "preproc_paste.h"

#define MAX_MACRO_DEPTH 4096

/*
 * Expansion algorithm overview
 * ----------------------------
 * Lines are scanned token by token.  When a potential macro name is
 * seen, `parse_macro_invocation` validates the call and dispatches to a
 * builtin or user-defined macro.  User-defined macros are expanded
 * recursively through `expand_user_macro` which, after optional argument
 * parsing, invokes the macro body via `expand_macro_call`.
 *
 * Each recursive expansion increments the DEPTH parameter.  To guard
 * against infinite recursion, calls fail when DEPTH reaches
 * `MAX_MACRO_DEPTH`.  Callers must propagate negative return values to
 * signal fatal errors while a zero return indicates malformed input
 * that should be copied verbatim.
 */

/* check expanded size against context limit */
static int check_expand_limit(strbuf_t *sb, preproc_context_t *ctx)
{
    if (ctx->max_expand_size && sb->len > ctx->max_expand_size) {
        fprintf(stderr, "Macro expansion size limit exceeded\n");
        return 0;
    }
    return 1;
}

static int expand_macro_call(macro_t *m, char **args, vector_t *macros,
                             strbuf_t *out, int depth,
                             preproc_context_t *ctx)
{
    strbuf_t tmp;
    strbuf_init(&tmp);
    int ok;
    if (m->params.count || m->variadic) {
        char *body = expand_params(m->value, &m->params, args, m->variadic);
        ok = expand_line(body, macros, &tmp, preproc_get_column(ctx), depth, ctx);
        free(body);
    } else {
        ok = expand_line(m->value, macros, &tmp, preproc_get_column(ctx), depth, ctx);
    }
    if (ok) {
        strbuf_append(out, tmp.data ? tmp.data : "");
        if (!check_expand_limit(out, ctx)) {
            strbuf_free(&tmp);
            return 0;
        }
    }
    strbuf_free(&tmp);
    return ok;
}

/* Emit a literal character and advance the input index. */
static void emit_plain_char(const char *line, size_t *pos, strbuf_t *out)
{
    strbuf_appendf(out, "%c", line[*pos]);
    (*pos)++;
}

/* Copy a quoted string or character literal verbatim. */
static void emit_quoted(const char *line, size_t *pos, char quote,
                        strbuf_t *out)
{
    strbuf_appendf(out, "%c", quote);
    (*pos)++;
    while (line[*pos]) {
        char c = line[*pos];
        strbuf_appendf(out, "%c", c);
        (*pos)++;
        if (c == '\\' && line[*pos]) {
            strbuf_appendf(out, "%c", line[*pos]);
            (*pos)++;
            continue;
        }
        if (c == quote)
            break;
    }
}


/* Return the macro whose name matches "name" (len bytes) */
static macro_t *find_macro(vector_t *macros, const char *name, size_t len)
{
    for (size_t i = 0; i < macros->count; i++) {
        macro_t *m = &((macro_t *)macros->data)[i];
        if (strlen(m->name) == len && strncmp(m->name, name, len) == 0)
            return m;
    }
    return NULL;
}


static void free_macro_args(char **ap, char *va, int variadic)
{
    if (variadic) {
        free(va);
        free(ap);
    }
}

/*
 * Parse the arguments for macro M starting at *pos in LINE.
 *
 * When successful the argument array and optional variadic string are
 * returned via *out_ap and *out_va and the argument vector is stored
 * in *out_args for later cleanup.  *pos is updated to the index after
 * the closing parenthesis.  Returns 1 on success, 0 when the call is
 * malformed and -1 on failure.
 */
static int parse_macro_arguments(macro_t *m, const char *line, size_t *pos,
                                 char ***out_ap, char **out_va,
                                 vector_t *out_args)
{
    if (!parse_macro_arg_vector(line, pos, out_args, m->params.count,
                                m->variadic))
        return 0;

    if (!handle_varargs(out_args, m->params.count, m->variadic,
                        out_ap, out_va)) {
        free_arg_vector(out_args);
        return -1;
    }
    return 1;
}

/*
 * Expand the body of macro M using the provided argument array.
 * The result is appended to OUT.  Returns 1 on success or -1 on
 * expansion failure.
 */
static int invoke_macro_body(macro_t *m, char **ap, vector_t *macros,
                             strbuf_t *out, int depth,
                             preproc_context_t *ctx)
{
    if (!expand_macro_call(m, ap, macros, out, depth + 1, ctx))
        return -1;
    return 1;
}

/*
 * Attempt to parse and expand a macro invocation starting at *pos.
 * On success the resulting text is appended to "out" and *pos is
 * updated to the index following the macro call.  Returns non-zero
 * when a macro expansion occurred.
 */

/* Expand a user-defined macro.  "pos" should point to the index right
 * after the macro name.  When expansion succeeds *pos is updated to the
 * index after the invocation and 1 is returned.  If the invocation was
 * malformed zero is returned.  On failure a negative value is returned. */
static int expand_user_macro(macro_t *m, const char *line, size_t *pos,
                             vector_t *macros, strbuf_t *out, int depth,
                             preproc_context_t *ctx)
{
    if (m->expanding) {
        int saved_errno = errno;
        size_t p = *pos;
        strbuf_append(out, m->name);
        if ((m->params.count || m->variadic) && line[p] == '(') {
            size_t depth = 0;
            do {
                strbuf_appendf(out, "%c", line[p]);
                if (line[p] == '(')
                    depth++;
                else if (line[p] == ')')
                    depth--;
                p++;
            } while (line[p - 1] && depth > 0);
        }
        *pos = p;
        errno = saved_errno;
        return 1;
    }

    size_t p = *pos;        /* position just after the macro name */

    /* Parse the invocation arguments when the macro expects them. */
    vector_t args;
    char **ap = NULL;
    char *va = NULL;
    int r = 1;
    if (m->params.count || m->variadic) {
        r = parse_macro_arguments(m, line, &p, &ap, &va, &args);
        if (r <= 0)
            return r;
    }

    m->expanding = 1;

    /* Invoke the macro body once arguments have been collected. */
    if (invoke_macro_body(m, ap, macros, out, depth, ctx) < 0) {
        if (m->params.count || m->variadic) {
            free_macro_args(ap, va, m->variadic);
            free_arg_vector(&args);
        }
        m->expanding = 0;
        return -1;
    }

    if (m->params.count || m->variadic) {
        free_macro_args(ap, va, m->variadic);
        free_arg_vector(&args);
    }
    *pos = p;
    m->expanding = 0;
    return 1;
}

/* Scan an identifier starting at POS and return the index after it.
 * "out_len" receives the identifier length when non-NULL.  Returns POS
 * when no identifier is present. */
static size_t read_macro_ident(const char *line, size_t pos, size_t *out_len)
{
    size_t len = parse_ident(line + pos);
    if (!len)
        return pos;
    if (out_len)
        *out_len = len;
    return pos + len;
}

/* Dispatch the identifier between builtin and user-defined macros. */
static int dispatch_macro(const char *line, size_t start, size_t end,
                          size_t len, vector_t *macros, strbuf_t *out,
                          size_t column, int depth, size_t *pos,
                          preproc_context_t *ctx)
{
    int r = handle_builtin_macro(line + start, len, end, column, out, pos, ctx);
    if (r)
        return r;

    macro_t *m = find_macro(macros, line + start, len);
    if (!m)
        return 0;

    preproc_set_location(ctx, NULL, preproc_get_line(ctx), column);
    size_t p = end;
    r = expand_user_macro(m, line, &p, macros, out, depth, ctx);
    if (r > 0)
        *pos = p;
    return r;
}
/*
 * Append the character represented by the escape sequence starting with
 * backslash C.  Additional characters are read from S beginning at *I and the
 * index is updated past any consumed input.
 */
static void append_escape_sequence(char c, const char *s, size_t *i,
                                   strbuf_t *sb)
{
    switch (c) {
    case 'n': strbuf_append(sb, "\n"); break;
    case 't': strbuf_append(sb, "\t"); break;
    case 'r': strbuf_append(sb, "\r"); break;
    case 'b': strbuf_append(sb, "\b"); break;
    case 'f': strbuf_append(sb, "\f"); break;
    case 'v': strbuf_append(sb, "\v"); break;
    case 'a': strbuf_append(sb, "\a"); break;
    case '\\':
    case '\'':
    case '"':
    case '?':
        strbuf_appendf(sb, "%c", c);
        break;
    case 'x': {
        unsigned value = 0;
        while (isxdigit((unsigned char)s[*i])) {
            char d = s[*i];
            int hex = (d >= '0' && d <= '9') ? d - '0' :
                       (d >= 'a' && d <= 'f') ? d - 'a' + 10 :
                       (d >= 'A' && d <= 'F') ? d - 'A' + 10 : 0;
            value = value * 16 + (unsigned)hex;
            (*i)++;
        }
        strbuf_appendf(sb, "%c", (char)value);
        break;
    }
    default:
        if (c >= '0' && c <= '7') {
            unsigned value = (unsigned)(c - '0');
            int digits = 1;
            while (digits < 3 && s[*i] >= '0' && s[*i] <= '7') {
                value = value * 8 + (unsigned)(s[*i] - '0');
                (*i)++; digits++;
            }
            strbuf_appendf(sb, "%c", (char)value);
        } else {
            strbuf_appendf(sb, "%c", c);
        }
        break;
    }
}

/* Decode escape sequences in a string literal and return a malloc'd copy. */
static char *decode_string_literal(const char *s, size_t len)
{
    strbuf_t sb;
    strbuf_init(&sb);

    for (size_t i = 0; i < len;) {
        char c = s[i];

        if (c == '\\' && i + 1 < len) {
            i++;
            c = s[i++];
            append_escape_sequence(c, s, &i, &sb);
        } else {
            strbuf_appendf(&sb, "%c", c);
            i++;
        }
    }

    char *res = vc_strdup(sb.data ? sb.data : "");
    strbuf_free(&sb);
    if (!res)
        vc_oom();
    return res;
}

/* Recognize and expand the _Pragma operator. */
static int handle_pragma_operator(const char *line, size_t *pos,
                                 strbuf_t *out)
{
    size_t start = *pos;
    size_t len = parse_ident(line + start);
    if (len != 7 || strncmp(line + start, "_Pragma", 7) != 0)
        return 0;
    size_t p = start + len;
    while (line[p] == ' ' || line[p] == '\t')
        p++;
    if (line[p] != '(')
        return 0;
    p++;
    while (line[p] == ' ' || line[p] == '\t')
        p++;
    if (line[p] != '"')
        return 0;
    p++;
    size_t str_start = p;
    while (line[p]) {
        if (line[p] == '\\' && line[p + 1]) {
            p += 2;
            continue;
        }
        if (line[p] == '"')
            break;
        p++;
    }
    if (line[p] != '"')
        return -1;
    size_t str_len = p - str_start;
    char *pragma = decode_string_literal(line + str_start, str_len);
    if (!pragma)
        return -1;
    p++;
    while (line[p] == ' ' || line[p] == '\t')
        p++;
    if (line[p] != ')') {
        free(pragma);
        return -1;
    }
    p++;
    int r = strbuf_appendf(out, "\n#pragma %s\n", pragma);
    free(pragma);
    if (r != 0)
        return -1;
    *pos = p;
    return 1;
}

/*
 * Attempt to expand a macro invocation starting at *pos.
 *
 * Returns 1 when a macro was successfully expanded, 0 when no
 * invocation was present and -1 on failure.  The recursion depth is
 * checked against MAX_MACRO_DEPTH and exceeding it reports an error.
 */
static int parse_macro_invocation(const char *line, size_t *pos,
                                  vector_t *macros, strbuf_t *out,
                                  size_t column, int depth,
                                  preproc_context_t *ctx)
{
    if (depth >= MAX_MACRO_DEPTH) {
        fprintf(stderr, "Macro expansion limit exceeded\n");
        return -1;
    }

    size_t start = *pos;
    int r = handle_pragma_operator(line, pos, out);
    if (r)
        return r;

    size_t len;
    size_t end = read_macro_ident(line, start, &len);
    if (end == start)
        return 0;

    return dispatch_macro(line, start, end, len, macros, out, column, depth,
                          pos, ctx);
}

/*
 * Expand all macros found in a single line of input.
 * Performs recursive expansion so macro bodies may contain other
 * macros.  Results are appended to the provided output buffer.
 */
static int expand_token(const char *line, size_t *pos, vector_t *macros,
                        strbuf_t *out, size_t column, int depth,
                        preproc_context_t *ctx)
{
    int r = parse_macro_invocation(line, pos, macros, out, column, depth, ctx);
    if (r < 0)
        return 0;
    if (!r)
        emit_plain_char(line, pos, out);
    return 1;
}

/*
 * Recursively expand all macros found in LINE and append the result to OUT.
 *
 * Returns 1 on success or 0 if a fatal error occurs.  DEPTH limits the
 * level of nested expansions and is checked against MAX_MACRO_DEPTH.
 */
int expand_line(const char *line, vector_t *macros, strbuf_t *out,
                size_t column, int depth, preproc_context_t *ctx)
{
    if (depth >= MAX_MACRO_DEPTH) {
        fprintf(stderr, "Macro expansion limit exceeded\n");
        return 0;
    }
    for (size_t i = 0; line[i];) {
        char c = line[i];
        if (c == '"' || c == '\'') {
            emit_quoted(line, &i, c, out);
            continue;
        }
        size_t col = column ? column : i + 1;
        if (!expand_token(line, &i, macros, out, col, depth, ctx))
            return 0;
        if (!check_expand_limit(out, ctx))
            return 0;
    }
    return 1;
}
