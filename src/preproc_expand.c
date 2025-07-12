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

#define MAX_MACRO_DEPTH 4096

/*
 * Remove trailing spaces or tabs from a string buffer.
 * Used when concatenating macro tokens with the ## operator.
 */
static void trim_trailing_ws(strbuf_t *sb)
{
    while (sb->len > 0 &&
           (sb->data[sb->len - 1] == ' ' || sb->data[sb->len - 1] == '\t')) {
        sb->len--;
        sb->data[sb->len] = '\0';
    }
}

/* Parse an identifier and return its length. */
static size_t parse_ident(const char *s)
{
    size_t i = 0;
    if (!isalpha((unsigned char)s[i]) && s[i] != '_')
        return 0;
    i++;
    while (isalnum((unsigned char)s[i]) || s[i] == '_')
        i++;
    return i;
}

/* Look up a parameter name and return the argument string if found. */
static const char *lookup_param(const char *name, size_t len,
                               const vector_t *params, char **args)
{
    for (size_t p = 0; p < params->count; p++) {
        const char *param = ((char **)params->data)[p];
        if (strlen(param) == len && strncmp(param, name, len) == 0)
            return args[p];
    }
    return NULL;
}

/*
 * Append the stringized form of a macro parameter referenced by the
 * `#` operator starting at index `i`.  The index after the processed
 * sequence is returned.
 */
static size_t append_stringized_param(const char *value, size_t i,
                                      const vector_t *params, char **args,
                                      int variadic, strbuf_t *sb)
{
    size_t j = i + 1;
    while (value[j] == ' ' || value[j] == '\t')
        j++;
    size_t len = parse_ident(value + j);
    if (len) {
        const char *rep = lookup_param(value + j, len, params, args);
        if (!rep && variadic && len == 11 &&
            strncmp(value + j, "__VA_ARGS__", 11) == 0)
            rep = args[params->count];
        if (rep) {
            strbuf_append(sb, "\"");
            for (size_t k = 0; rep[k]; k++) {
                char c = rep[k];
                if (c == '\\' || c == '"')
                    strbuf_appendf(sb, "\\%c", c);
                else
                    strbuf_appendf(sb, "%c", c);
            }
            strbuf_append(sb, "\"");
            return j + len;
        }
    }
    strbuf_append(sb, "#");
    return i + 1;
}

/*
 * Perform the `##` token-pasting operation starting at the identifier
 * beginning at `i`.  `len` and `rep` describe this first identifier.
 * When pasting occurs `*out_i` is set to the index after the sequence
 * and non-zero is returned.
 */
static int append_pasted_tokens(const char *value, size_t i, size_t len,
                                const char *rep, const vector_t *params,
                                char **args, int variadic, strbuf_t *sb,
                                size_t *out_i)
{
    size_t k = i + len;
    while (value[k] == ' ' || value[k] == '\t')
        k++;
    if (value[k] != '#' || value[k + 1] != '#')
        return 0;
    k += 2;
    while (value[k] == ' ' || value[k] == '\t')
        k++;

    if (isalpha((unsigned char)value[k]) || value[k] == '_') {
        size_t l = parse_ident(value + k);
        const char *rep2 = lookup_param(value + k, l, params, args);
        if (!rep2 && variadic && l == 11 &&
            strncmp(value + k, "__VA_ARGS__", 11) == 0)
            rep2 = args[params->count];
        trim_trailing_ws(sb);
        if (rep)
            strbuf_append(sb, rep);
        else
            strbuf_appendf(sb, "%.*s", (int)len, value + i);
        if (rep2)
            strbuf_append(sb, rep2);
        else
            strbuf_appendf(sb, "%.*s", (int)l, value + k);
        *out_i = k + l;
    } else {
        trim_trailing_ws(sb);
        if (rep)
            strbuf_append(sb, rep);
        else
            strbuf_appendf(sb, "%.*s", (int)len, value + i);
        strbuf_appendf(sb, "%c", value[k]);
        *out_i = k + 1;
    }
    return 1;
}

/*
 * Expand parameter references inside a macro body.
 * Handles substitution, stringification (#) and token pasting (##)
 * using the provided argument array.
 */
static char *expand_params(const char *value, const vector_t *params, char **args,
                           int variadic)
{
    strbuf_t sb;
    strbuf_init(&sb);
    /*
     * Walk the macro body one character at a time performing the
     * appropriate substitution or copying each literal character.
     */
    for (size_t i = 0; value[i];) {
        /*
         * Detect the `#` stringizing operator.  When a single `#` is
         * found, convert the following macro parameter to a quoted
         * string.
         */
        if (value[i] == '#' && value[i + 1] != '#') {
            i = append_stringized_param(value, i, params, args, variadic, &sb);
            continue;
        }

        size_t len = parse_ident(value + i);
        if (len) {
            /* Look up the identifier as a macro parameter. */
            const char *rep = lookup_param(value + i, len, params, args);
            if (!rep && variadic && len == 11 &&
                strncmp(value + i, "__VA_ARGS__", 11) == 0)
                rep = args[params->count];
            size_t next;
            /*
             * Check for the `##` token pasting operator starting at this
             * identifier and handle concatenation when present.
             */
            if (append_pasted_tokens(value, i, len, rep, params, args, variadic, &sb, &next)) {
                i = next;
                continue;
            }

            /*
             * If the name matched a parameter copy the replacement
             * text, otherwise copy the identifier verbatim.
             */
            if (rep)
                strbuf_append(&sb, rep);
            else
                strbuf_appendf(&sb, "%.*s", (int)len, value + i);
            i += len;
            continue;
        }

        /* No identifier or operator - emit the character as plain text */
        strbuf_appendf(&sb, "%c", value[i]);
        i++;
    }
    char *out = vc_strdup(sb.data ? sb.data : "");
    strbuf_free(&sb);
    return out;
}


/* Expand a macro invocation and append the result to "out". */
static int expand_macro_call(macro_t *m, char **args, vector_t *macros,
                             strbuf_t *out, int depth)
{
    strbuf_t tmp;
    strbuf_init(&tmp);
    int ok;
    if (m->params.count || m->variadic) {
        char *body = expand_params(m->value, &m->params, args, m->variadic);
        ok = expand_line(body, macros, &tmp, preproc_get_column(), depth);
        free(body);
    } else {
        ok = expand_line(m->value, macros, &tmp, preproc_get_column(), depth);
    }
    if (ok)
        strbuf_append(out, tmp.data ? tmp.data : "");
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
                             strbuf_t *out, int depth)
{
    if (!expand_macro_call(m, ap, macros, out, depth + 1))
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
                             vector_t *macros, strbuf_t *out, int depth)
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
    if (invoke_macro_body(m, ap, macros, out, depth) < 0) {
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
                          size_t column, int depth, size_t *pos)
{
    int r = handle_builtin_macro(line + start, len, end, column, out, pos);
    if (r)
        return r;

    macro_t *m = find_macro(macros, line + start, len);
    if (!m)
        return 0;

    preproc_set_location(NULL, preproc_get_line(), column);
    size_t p = end;
    r = expand_user_macro(m, line, &p, macros, out, depth);
    if (r > 0)
        *pos = p;
    return r;
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
            switch (c) {
            case 'n': strbuf_append(&sb, "\n"); break;
            case 't': strbuf_append(&sb, "\t"); break;
            case 'r': strbuf_append(&sb, "\r"); break;
            case 'b': strbuf_append(&sb, "\b"); break;
            case 'f': strbuf_append(&sb, "\f"); break;
            case 'v': strbuf_append(&sb, "\v"); break;
            case 'a': strbuf_append(&sb, "\a"); break;
            case '\\':
            case '\'':
            case '"':
            case '?':
                strbuf_appendf(&sb, "%c", c);
                break;
            case 'x': {
                unsigned value = 0;
                int digits = 0;
                while (i < len && isxdigit((unsigned char)s[i])) {
                    char d = s[i];
                    int hex = (d >= '0' && d <= '9') ? d - '0' :
                               (d >= 'a' && d <= 'f') ? d - 'a' + 10 :
                               (d >= 'A' && d <= 'F') ? d - 'A' + 10 : 0;
                    value = value * 16 + (unsigned)hex;
                    i++; digits++;
                }
                strbuf_appendf(&sb, "%c", (char)value);
                break;
            }
            default:
                if (c >= '0' && c <= '7') {
                    unsigned value = (unsigned)(c - '0');
                    int digits = 1;
                    while (digits < 3 && i < len && s[i] >= '0' && s[i] <= '7') {
                        value = value * 8 + (unsigned)(s[i] - '0');
                        i++; digits++;
                    }
                    strbuf_appendf(&sb, "%c", (char)value);
                } else {
                    strbuf_appendf(&sb, "%c", c);
                }
                break;
            }
        } else {
            strbuf_appendf(&sb, "%c", c);
            i++;
        }
    }

    char *res = vc_strdup(sb.data ? sb.data : "");
    strbuf_free(&sb);
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

static int parse_macro_invocation(const char *line, size_t *pos,
                                  vector_t *macros, strbuf_t *out,
                                  size_t column, int depth)
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
                          pos);
}

/*
 * Expand all macros found in a single line of input.
 * Performs recursive expansion so macro bodies may contain other
 * macros.  Results are appended to the provided output buffer.
 */
static int expand_token(const char *line, size_t *pos, vector_t *macros,
                        strbuf_t *out, size_t column, int depth)
{
    int r = parse_macro_invocation(line, pos, macros, out, column, depth);
    if (r < 0)
        return 0;
    if (!r)
        emit_plain_char(line, pos, out);
    return 1;
}

int expand_line(const char *line, vector_t *macros, strbuf_t *out,
                size_t column, int depth)
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
        if (!expand_token(line, &i, macros, out, col, depth))
            return 0;
    }
    return 1;
}
