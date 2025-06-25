/*
 * Minimal preprocessing implementation.
 *
 * Handles '#include "file"', object-like '#define' and simple single argument
 * macros '#define NAME(arg)'. Expansion performs naive text substitution with
 * no awareness of strings or comments. Nested includes are supported but there
 * are no include guards.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preproc.h"
#include "util.h"
#include "vector.h"
#include "strbuf.h"

/* Stored macro definition */
typedef struct {
    char *name;
    char *param; /* NULL for object-like macros */
    char *value;
} macro_t;

/* Free memory for a macro */
static void macro_free(macro_t *m)
{
    if (!m) return;
    free(m->name);
    free(m->param);
    free(m->value);
}

/* Replace identifiers in 'line' with macro values into 'out'.
 * This does no tokenization and simply matches whole identifiers.
 */
/* Expand a parameterized macro value by substituting 'arg' for 'param'. */
static char *expand_param_value(const char *value, const char *param, const char *arg)
{
    strbuf_t sb;
    strbuf_init(&sb);
    for (size_t i = 0; value[i];) {
        if (isalpha((unsigned char)value[i]) || value[i] == '_') {
            size_t j = i + 1;
            while (isalnum((unsigned char)value[j]) || value[j] == '_')
                j++;
            size_t len = j - i;
            if (strlen(param) == len && strncmp(param, value + i, len) == 0) {
                strbuf_append(&sb, arg);
            } else {
                strbuf_appendf(&sb, "%.*s", (int)len, value + i);
            }
            i = j;
        } else {
            strbuf_appendf(&sb, "%c", value[i]);
            i++;
        }
    }
    char *out = vc_strdup(sb.data ? sb.data : "");
    strbuf_free(&sb);
    return out;
}

static void expand_line(const char *line, vector_t *macros, strbuf_t *out)
{
    for (size_t i = 0; line[i];) {
        int replaced = 0;
        if (isalpha((unsigned char)line[i]) || line[i] == '_') {
            size_t j = i + 1;
            while (isalnum((unsigned char)line[j]) || line[j] == '_')
                j++;
            size_t len = j - i;
            for (size_t k = 0; k < macros->count; k++) {
                macro_t *m = &((macro_t *)macros->data)[k];
                if (strlen(m->name) == len && strncmp(m->name, line + i, len) == 0) {
                    if (m->param) {
                        size_t p = j;
                        while (line[p] == ' ' || line[p] == '\t')
                            p++;
                        if (line[p] == '(') {
                            p++;
                            size_t start = p;
                            while (line[p] && line[p] != ')')
                                p++;
                            if (line[p] == ')') {
                                char *arg = vc_strndup(line + start, p - start);
                                char *exp = expand_param_value(m->value, m->param, arg);
                                strbuf_append(out, exp);
                                free(arg);
                                free(exp);
                                i = p + 1;
                                replaced = 1;
                                break;
                            }
                        }
                        /* fallthrough if no parentheses */
                    } else {
                        strbuf_append(out, m->value);
                        i = j;
                        replaced = 1;
                        break;
                    }
                }
            }
        }
        if (!replaced) {
            strbuf_appendf(out, "%c", line[i]);
            i++;
        }
    }
}

/* Process a single file, writing output to 'out'. */
static int process_file(const char *path, vector_t *macros, strbuf_t *out)
{
    char *text = vc_read_file(path);
    if (!text)
        return 0;
    char *dir = NULL;
    const char *slash = strrchr(path, '/');
    if (slash) {
        size_t len = (size_t)(slash - path) + 1;
        dir = vc_strndup(path, len);
    }

    char *line = strtok(text, "\n");
    while (line) {
        while (*line == ' ' || *line == '\t')
            line++;
        if (strncmp(line, "#include", 8) == 0 && (line[8] == ' ' || line[8] == '\t')) {
            char *quote = strchr(line, '"');
            char *end = quote ? strchr(quote + 1, '"') : NULL;
            if (quote && end) {
                size_t len = (size_t)(end - quote - 1);
                char incpath[512];
                if (dir)
                    snprintf(incpath, sizeof(incpath), "%s%.*s", dir, (int)len, quote + 1);
                else
                    snprintf(incpath, sizeof(incpath), "%.*s", (int)len, quote + 1);
                if (!process_file(incpath, macros, out)) {
                    free(text);
                    free(dir);
                    return 0;
                }
            }
        } else if (strncmp(line, "#define", 7) == 0 && (line[7] == ' ' || line[7] == '\t')) {
            char *n = line + 7;
            while (*n == ' ' || *n == '\t')
                n++;
            char *name = n;
            while (*n && !isspace((unsigned char)*n) && *n != '(')
                n++;
            char *param = NULL;
            if (*n == '(') {
                *n++ = '\0';
                char *pstart = n;
                while (*n && *n != ')')
                    n++;
                if (*n == ')') {
                    param = vc_strndup(pstart, (size_t)(n - pstart));
                    n++; /* skip ')' */
                } else {
                    /* malformed macro, treat as object-like */
                    n = pstart - 1; /* restore '(' position */
                }
            } else if (*n) {
                *n++ = '\0';
            }
            while (*n == ' ' || *n == '\t')
                n++;
            char *val = *n ? n : "";
            macro_t m = { vc_strdup(name), param, vc_strdup(val) };
            if (!vector_push(macros, &m)) {
                macro_free(&m);
                free(text);
                free(dir);
                return 0;
            }
        } else {
            strbuf_t tmp;
            strbuf_init(&tmp);
            expand_line(line, macros, &tmp);
            strbuf_append(&tmp, "\n");
            strbuf_append(out, tmp.data);
            strbuf_free(&tmp);
        }
        line = strtok(NULL, "\n");
    }
    free(text);
    free(dir);
    return 1;
}

/* Entry point: preprocess the file and return a newly allocated buffer. */
char *preproc_run(const char *path)
{
    vector_t macros;
    vector_init(&macros, sizeof(macro_t));
    strbuf_t out;
    strbuf_init(&out);
    int ok = process_file(path, &macros, &out);
    for (size_t i = 0; i < macros.count; i++)
        macro_free(&((macro_t *)macros.data)[i]);
    vector_free(&macros);
    char *res = NULL;
    if (ok)
        res = vc_strdup(out.data ? out.data : "");
    strbuf_free(&out);
    return res;
}

