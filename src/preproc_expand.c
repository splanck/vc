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
#include "util.h"
#include "vector.h"
#include "strbuf.h"
#include "preproc_args.h"
#include "preproc_builtin.h"

#define MAX_MACRO_DEPTH 100

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
            strbuf_appendf(sb, "\"%s\"", rep);
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
    for (size_t i = 0; value[i];) {
        if (value[i] == '#' && value[i + 1] != '#') {
            i = append_stringized_param(value, i, params, args, variadic, &sb);
            continue;
        }

        size_t len = parse_ident(value + i);
        if (len) {
            const char *rep = lookup_param(value + i, len, params, args);
            if (!rep && variadic && len == 11 &&
                strncmp(value + i, "__VA_ARGS__", 11) == 0)
                rep = args[params->count];
            size_t next;
            if (append_pasted_tokens(value, i, len, rep, params, args, variadic, &sb, &next)) {
                i = next;
                continue;
            }

            if (rep)
                strbuf_append(&sb, rep);
            else
                strbuf_appendf(&sb, "%.*s", (int)len, value + i);
            i += len;
            continue;
        }

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
 * Attempt to parse and expand a macro invocation starting at *pos.
 * On success the resulting text is appended to "out" and *pos is
 * updated to the index following the macro call.  Returns non-zero
 * when a macro expansion occurred.

/* Expand a user-defined macro.  "pos" should point to the index right
 * after the macro name.  When expansion succeeds *pos is updated to the
 * index after the invocation and 1 is returned.  If the invocation was
 * malformed zero is returned.  On failure a negative value is returned. */
static int expand_user_macro(macro_t *m, const char *line, size_t *pos,
                             vector_t *macros, strbuf_t *out, int depth)
{
    size_t p = *pos;
    if (m->params.count || m->variadic) {
        vector_t args;
        if (parse_macro_args(line, &p, &args) &&
            ((m->variadic && args.count >= m->params.count) ||
             (!m->variadic && args.count == m->params.count))) {
            char **ap = (char **)args.data;
            char *va = NULL;
            if (m->variadic &&
                !gather_varargs(&args, m->params.count, &ap, &va)) {
                free_arg_vector(&args);
                return -1;
            }
            if (!expand_macro_call(m, ap, macros, out, depth + 1)) {
                free_arg_vector(&args);
                free_macro_args(ap, va, m->variadic);
                return -1;
            }
            free_arg_vector(&args);
            free_macro_args(ap, va, m->variadic);
            *pos = p;
            return 1;
        }
        free_arg_vector(&args);
        return 0;
    }
    if (!expand_macro_call(m, NULL, macros, out, depth + 1))
        return -1;
    *pos = p;
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

static int parse_macro_invocation(const char *line, size_t *pos,
                                  vector_t *macros, strbuf_t *out,
                                  size_t column, int depth)
{
    if (depth >= MAX_MACRO_DEPTH) {
        fprintf(stderr, "Macro expansion limit exceeded\n");
        return -1;
    }

    size_t start = *pos;
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
        size_t col = column ? column : i + 1;
        if (!expand_token(line, &i, macros, out, col, depth))
            return 0;
    }
    return 1;
}
