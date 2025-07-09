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
#include <stdio.h>
#include "util.h"
#include "vector.h"
#include "strbuf.h"

#define MAX_MACRO_DEPTH 100

/* current expansion location for builtin macros */
static const char *builtin_file = "";
static size_t builtin_line = 0;
static size_t builtin_column = 1;
static const char *builtin_func = NULL;

static const char build_date[] = __DATE__;
static const char build_time[] = __TIME__;

void preproc_set_location(const char *file, size_t line, size_t column)
{
    if (file)
        builtin_file = file;
    builtin_line = line;
    builtin_column = column;
}

void preproc_set_function(const char *name)
{
    builtin_func = name;
}

/*
 * Release all memory associated with a macro definition.
 *
 * The macro must have been created by add_macro() which allocates
 * the name and value strings and takes ownership of all parameter
 * names.  macro_free() disposes of these heap allocations and resets
 * the internal vector.  It is safe to pass NULL.
 */
void macro_free(macro_t *m)
{
    if (!m)
        return;
    free(m->name);
    for (size_t i = 0; i < m->params.count; i++)
        free(((char **)m->params.data)[i]);
    vector_free(&m->params);
    free(m->value);
}

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

/* Parse the argument list for a parameterized macro call.
 *
 * "pos" should point to the character immediately following the macro
 * name.  On success the parsed arguments are stored in "out" and the
 * index after the closing ')' is written to *pos.  Returns non-zero when
 * a well formed argument list was found. */
static int parse_macro_args(const char *line, size_t *pos, vector_t *out)
{
    size_t p = *pos;
    vector_init(out, sizeof(char *));
    while (line[p] == ' ' || line[p] == '\t')
        p++;
    if (line[p] != '(')
        return 0;
    p++; /* skip '(' */
    while (line[p] == ' ' || line[p] == '\t')
        p++;
    if (line[p] == ')') {
        *pos = p + 1;
        return 1; /* no arguments */
    }

    size_t arg_start = p;
    int nest = 0;
    for (;; p++) {
        char c = line[p];
        if (c == '\0')
            return 0; /* unmatched '(' */
        if (c == '(') {
            nest++;
            continue;
        }
        if (c == ')') {
            if (nest > 0) {
                nest--;
                continue;
            }
            size_t a = arg_start;
            while (a < p && (line[a] == ' ' || line[a] == '\t'))
                a++;
            size_t end = p;
            while (end > a && (line[end - 1] == ' ' || line[end - 1] == '\t'))
                end--;
            char *dup = vc_strndup(line + a, end - a);
            if (!dup || !vector_push(out, &dup)) {
                free(dup);
                goto fail;
            }
            *pos = p + 1;
            return 1;
        }
        if (c == ',' && nest == 0) {
            size_t a = arg_start;
            while (a < p && (line[a] == ' ' || line[a] == '\t'))
                a++;
            size_t end = p;
            while (end > a && (line[end - 1] == ' ' || line[end - 1] == '\t'))
                end--;
            char *dup = vc_strndup(line + a, end - a);
            if (!dup || !vector_push(out, &dup)) {
                free(dup);
                goto fail;
            }
            p++; /* skip ',' */
            while (line[p] == ' ' || line[p] == '\t')
                p++;
            arg_start = p;
            p--; /* balance for loop increment */
            continue;
        }
    }
fail:
    for (size_t i = 0; i < out->count; i++)
        free(((char **)out->data)[i]);
    vector_free(out);
    return 0;
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
        ok = expand_line(body, macros, &tmp, builtin_column, depth);
        free(body);
    } else {
        ok = expand_line(m->value, macros, &tmp, builtin_column, depth);
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

/*
 * Attempt to parse and expand a macro invocation starting at *pos.
 * On success the resulting text is appended to "out" and *pos is
 * updated to the index following the macro call.  Returns non-zero
 * when a macro expansion occurred.
 */
/* Expand builtin macros such as __FILE__ and __LINE__.  "name" is the
 * identifier text starting at line[*pos] with length "len" and "end"
 * is the index after the identifier.  On success *pos is updated to
 * "end" and non-zero is returned. */
static int handle_builtin_macro(const char *name, size_t len, size_t end,
                                size_t column, strbuf_t *out, size_t *pos)
{
    if (len == 8) {
        if (strncmp(name, "__FILE__", 8) == 0) {
            preproc_set_location(NULL, builtin_line, column);
            strbuf_appendf(out, "\"%s\"", builtin_file);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__LINE__", 8) == 0) {
            preproc_set_location(NULL, builtin_line, column);
            strbuf_appendf(out, "%zu", builtin_line);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__DATE__", 8) == 0) {
            preproc_set_location(NULL, builtin_line, column);
            strbuf_appendf(out, "\"%s\"", build_date);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__TIME__", 8) == 0) {
            preproc_set_location(NULL, builtin_line, column);
            strbuf_appendf(out, "\"%s\"", build_time);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__STDC__", 8) == 0) {
            preproc_set_location(NULL, builtin_line, column);
            strbuf_append(out, "1");
            *pos = end;
            return 1;
        } else if (strncmp(name, "__func__", 8) == 0) {
            if (builtin_func) {
                preproc_set_location(NULL, builtin_line, column);
                strbuf_appendf(out, "\"%s\"", builtin_func);
                *pos = end;
                return 1;
            }
        }
    } else if (len == 16 && strncmp(name, "__STDC_VERSION__", 16) == 0) {
        preproc_set_location(NULL, builtin_line, column);
        strbuf_append(out, "199901L");
        *pos = end;
        return 1;
    }
    return 0;
}

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
            if (m->variadic) {
                size_t fixed = m->params.count;
                strbuf_t sb;
                strbuf_init(&sb);
                for (size_t a = fixed; a < args.count; a++) {
                    if (a > fixed)
                        strbuf_append(&sb, ",");
                    strbuf_append(&sb, ((char **)args.data)[a]);
                }
                va = vc_strdup(sb.data ? sb.data : "");
                strbuf_free(&sb);
                ap = malloc((fixed + 1) * sizeof(char *));
                if (!ap) {
                    vc_oom();
                    for (size_t t = 0; t < args.count; t++)
                        free(((char **)args.data)[t]);
                    vector_free(&args);
                    free(va);
                    return -1;
                }
                for (size_t a = 0; a < fixed; a++)
                    ap[a] = ((char **)args.data)[a];
                ap[fixed] = va;
            }
            if (!expand_macro_call(m, ap, macros, out, depth + 1)) {
                for (size_t t = 0; t < args.count; t++)
                    free(((char **)args.data)[t]);
                vector_free(&args);
                if (m->variadic) {
                    free(va);
                    free(ap);
                }
                return -1;
            }
            for (size_t t = 0; t < args.count; t++)
                free(((char **)args.data)[t]);
            vector_free(&args);
            if (m->variadic) {
                free(va);
                free(ap);
            }
            *pos = p;
            return 1;
        }
        for (size_t t = 0; t < args.count; t++)
            free(((char **)args.data)[t]);
        vector_free(&args);
        return 0;
    } else {
        if (!expand_macro_call(m, NULL, macros, out, depth + 1))
            return -1;
        *pos = p;
        return 1;
    }
}

static int parse_macro_invocation(const char *line, size_t *pos,
                                  vector_t *macros, strbuf_t *out,
                                  size_t column, int depth)
{
    if (depth >= MAX_MACRO_DEPTH) {
        fprintf(stderr, "Macro expansion limit exceeded\n");
        return -1;
    }
    size_t i = *pos;

    if (!isalpha((unsigned char)line[i]) && line[i] != '_')
        return 0;

    size_t j = i + 1;
    while (isalnum((unsigned char)line[j]) || line[j] == '_')
        j++;

    size_t len = j - i;

    int r = handle_builtin_macro(line + i, len, j, column, out, pos);
    if (r)
        return r;
    for (size_t k = 0; k < macros->count; k++) {
        macro_t *m = &((macro_t *)macros->data)[k];
        if (strlen(m->name) == len && strncmp(m->name, line + i, len) == 0) {
            preproc_set_location(NULL, builtin_line, column);
            size_t pos2 = j;
            r = expand_user_macro(m, line, &pos2, macros, out, depth);
            if (r) {
                if (r > 0)
                    *pos = pos2;
                return r;
            }
            break;
        }
    }

    return 0;
}

/*
 * Expand all macros found in a single line of input.
 * Performs recursive expansion so macro bodies may contain other
 * macros.  Results are appended to the provided output buffer.
 */
int expand_line(const char *line, vector_t *macros, strbuf_t *out, size_t column, int depth)
{
    if (depth >= MAX_MACRO_DEPTH) {
        fprintf(stderr, "Macro expansion limit exceeded\n");
        return 0;
    }
    for (size_t i = 0; line[i];) {
        size_t col = column ? column : i + 1;
        int r = parse_macro_invocation(line, &i, macros, out, col, depth);
        if (r < 0)
            return 0;
        if (!r)
            emit_plain_char(line, &i, out);
    }
    return 1;
}

/*
 * Return non-zero if a macro with the given name exists in the
 * macro vector.
 */
int is_macro_defined(vector_t *macros, const char *name)
{
    if (strcmp(name, "__FILE__") == 0 || strcmp(name, "__LINE__") == 0 ||
        strcmp(name, "__DATE__") == 0 || strcmp(name, "__TIME__") == 0 ||
        strcmp(name, "__STDC__") == 0 || strcmp(name, "__STDC_VERSION__") == 0 ||
        strcmp(name, "__func__") == 0 || strcmp(name, "offsetof") == 0)
        return 1;

    for (size_t i = 0; i < macros->count; i++) {
        macro_t *m = &((macro_t *)macros->data)[i];
        if (strcmp(m->name, name) == 0)
            return 1;
    }
    return 0;
}

/*
 * Delete all macros matching the given name from the macro list.
 *
 * Each removed entry is cleaned up with macro_free().  The order of the
 * remaining macros is preserved.
 */
void remove_macro(vector_t *macros, const char *name)
{
    for (size_t i = 0; i < macros->count;) {
        macro_t *m = &((macro_t *)macros->data)[i];
        if (strcmp(m->name, name) == 0) {
            macro_free(m);
            if (i + 1 < macros->count)
                memmove(m, m + 1, (macros->count - i - 1) * sizeof(macro_t));
            macros->count--;
        } else {
            i++;
        }
    }
}

