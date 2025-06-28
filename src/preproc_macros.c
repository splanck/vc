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
#include "util.h"
#include "vector.h"
#include "strbuf.h"

/* current expansion location for builtin macros */
static const char *builtin_file = "";
static size_t builtin_line = 0;
static const char *builtin_func = NULL;

static const char build_date[] = __DATE__;
static const char build_time[] = __TIME__;

void preproc_set_location(const char *file, size_t line)
{
    builtin_file = file ? file : "";
    builtin_line = line;
}

void preproc_set_function(const char *name)
{
    builtin_func = name;
}

/*
 * Release all memory associated with a macro definition.
 * Frees the name string, parameter list and value.  Safe to
 * call with a NULL pointer.
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

/* Handle a `#` operator starting at `i`.  Appends to `sb` and
 * returns the index after the processed sequence. */
static size_t handle_stringify(const char *value, size_t i,
                               const vector_t *params, char **args,
                               strbuf_t *sb)
{
    size_t j = i + 1;
    while (value[j] == ' ' || value[j] == '\t')
        j++;
    size_t len = parse_ident(value + j);
    if (len) {
        const char *rep = lookup_param(value + j, len, params, args);
        if (rep) {
            strbuf_appendf(sb, "\"%s\"", rep);
            return j + len;
        }
    }
    strbuf_append(sb, "#");
    return i + 1;
}

/* Handle a `##` operator following the identifier that starts at `i`.
 * `len` and `rep` describe this first identifier.  Returns non-zero
 * when token pasting occurred and updates `*out_i` to the index after
 * the processed sequence. */
static int handle_token_paste(const char *value, size_t i, size_t len,
                              const char *rep, const vector_t *params,
                              char **args, strbuf_t *sb, size_t *out_i)
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
static char *expand_params(const char *value, const vector_t *params, char **args)
{
    strbuf_t sb;
    strbuf_init(&sb);
    for (size_t i = 0; value[i];) {
        if (value[i] == '#' && value[i + 1] != '#') {
            i = handle_stringify(value, i, params, args, &sb);
            continue;
        }

        size_t len = parse_ident(value + i);
        if (len) {
            const char *rep = lookup_param(value + i, len, params, args);
            size_t next;
            if (handle_token_paste(value, i, len, rep, params, args, &sb, &next)) {
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
    size_t start = p;
    while (line[p] && line[p] != ')')
        p++;
    if (line[p] != ')')
        return 0;

    char *argstr = vc_strndup(line + start, p - start);
    char *tok; char *sp;
    tok = strtok_r(argstr, ",", &sp);
    while (tok) {
        while (*tok == ' ' || *tok == '\t')
            tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t'))
            end--;
        char *a = vc_strndup(tok, (size_t)(end - tok));
        vector_push(out, &a);
        tok = strtok_r(NULL, ",", &sp);
    }
    free(argstr);
    *pos = p + 1;
    return 1;
}

/* Expand a macro invocation and append the result to "out". */
static void expand_macro_call(macro_t *m, char **args, vector_t *macros,
                              strbuf_t *out)
{
    strbuf_t tmp;
    strbuf_init(&tmp);
    if (m->params.count) {
        char *body = expand_params(m->value, &m->params, args);
        expand_line(body, macros, &tmp);
        free(body);
    } else {
        expand_line(m->value, macros, &tmp);
    }
    strbuf_append(out, tmp.data ? tmp.data : "");
    strbuf_free(&tmp);
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
static int parse_macro_invocation(const char *line, size_t *pos,
                                  vector_t *macros, strbuf_t *out)
{
    size_t i = *pos;

    if (!isalpha((unsigned char)line[i]) && line[i] != '_')
        return 0;

    size_t j = i + 1;
    while (isalnum((unsigned char)line[j]) || line[j] == '_')
        j++;

    size_t len = j - i;

    /* handle builtin macros first */
    if (len == 8) {
        if (strncmp(line + i, "__FILE__", 8) == 0) {
            strbuf_appendf(out, "\"%s\"", builtin_file);
            *pos = j;
            return 1;
        } else if (strncmp(line + i, "__LINE__", 8) == 0) {
            strbuf_appendf(out, "%zu", builtin_line);
            *pos = j;
            return 1;
        } else if (strncmp(line + i, "__DATE__", 8) == 0) {
            strbuf_appendf(out, "\"%s\"", build_date);
            *pos = j;
            return 1;
        } else if (strncmp(line + i, "__TIME__", 8) == 0) {
            strbuf_appendf(out, "\"%s\"", build_time);
            *pos = j;
            return 1;
        } else if (strncmp(line + i, "__STDC__", 8) == 0) {
            strbuf_append(out, "1");
            *pos = j;
            return 1;
        }
    } else if (len == 16 && strncmp(line + i, "__STDC_VERSION__", 16) == 0) {
        strbuf_append(out, "199901");
        *pos = j;
        return 1;
    }
    for (size_t k = 0; k < macros->count; k++) {
        macro_t *m = &((macro_t *)macros->data)[k];
        if (strlen(m->name) == len && strncmp(m->name, line + i, len) == 0) {
            if (m->params.count) {
                size_t pos2 = j;
                vector_t args;
                if (parse_macro_args(line, &pos2, &args) &&
                    args.count == m->params.count) {
                    expand_macro_call(m, (char **)args.data, macros, out);
                    for (size_t t = 0; t < args.count; t++)
                        free(((char **)args.data)[t]);
                    vector_free(&args);
                    *pos = pos2;
                    return 1;
                }
                for (size_t t = 0; t < args.count; t++)
                    free(((char **)args.data)[t]);
                vector_free(&args);
            } else {
                expand_macro_call(m, NULL, macros, out);
                *pos = j;
                return 1;
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
void expand_line(const char *line, vector_t *macros, strbuf_t *out)
{
    for (size_t i = 0; line[i];) {
        if (!parse_macro_invocation(line, &i, macros, out))
            emit_plain_char(line, &i, out);
    }
}

/*
 * Return non-zero if a macro with the given name exists in the
 * macro vector.
 */
int is_macro_defined(vector_t *macros, const char *name)
{
    if (strcmp(name, "__FILE__") == 0 || strcmp(name, "__LINE__") == 0 ||
        strcmp(name, "__DATE__") == 0 || strcmp(name, "__TIME__") == 0 ||
        strcmp(name, "__STDC__") == 0 || strcmp(name, "__STDC_VERSION__") == 0)
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
 * The order of remaining entries is preserved.
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

