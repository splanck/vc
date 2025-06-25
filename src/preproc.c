/*
 * Minimal preprocessing implementation.
 *
 * Only handles '#include "file"' and object-like '#define'.
 * Macros are expanded with simple text substitution and no awareness of
 * string literals or comments. Nested includes are supported but no
 * include guards are provided.
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "preproc.h"
#include "util.h"
#include "vector.h"
#include "strbuf.h"

/* Stored macro definition */
typedef struct {
    char *name;
    char *value;
} macro_t;

/* Free memory for a macro */
static void macro_free(macro_t *m)
{
    if (!m)
        return;
    free(m->name);
    free(m->value);
}

/* Add or update a macro definition */
static int macro_define(vector_t *macros, const char *name, const char *value)
{
    macro_t m = { vc_strdup(name), vc_strdup(value) };
    if (!m.name || !m.value || !vector_push(macros, &m)) {
        macro_free(&m);
        return 0;
    }
    return 1;
}

/* Look up a macro by identifier name */
static const char *macro_lookup(vector_t *macros, const char *name, size_t len)
{
    for (size_t i = 0; i < macros->count; i++) {
        macro_t *m = &((macro_t *)macros->data)[i];
        if (strlen(m->name) == len && strncmp(m->name, name, len) == 0)
            return m->value;
    }
    return NULL;
}

/* Replace identifiers in 'line' with macro values into 'out'.
 * This does no tokenization and simply matches whole identifiers.
 */
static void macro_expand_line(const char *line, vector_t *macros, strbuf_t *out)
{
    for (size_t i = 0; line[i];) {
        int replaced = 0;
        if (isalpha((unsigned char)line[i]) || line[i] == '_') {
            size_t j = i + 1;
            while (isalnum((unsigned char)line[j]) || line[j] == '_')
                j++;
            size_t len = j - i;
            const char *val = macro_lookup(macros, line + i, len);
            if (val) {
                strbuf_append(out, val);
                i = j;
                replaced = 1;
            }
        }
        if (!replaced) {
            strbuf_appendf(out, "%c", line[i]);
            i++;
        }
    }
}

/* Attempt to resolve an include file using the caller provided search paths. */
static int find_include(const char *name, const char *dir,
                        const char **search_paths, size_t num_paths,
                        char *out, size_t out_size)
{
    if (dir) {
        snprintf(out, out_size, "%s%s", dir, name);
        if (access(out, R_OK) == 0)
            return 1;
    }
    for (size_t i = 0; i < num_paths; i++) {
        snprintf(out, out_size, "%s/%s", search_paths[i], name);
        if (access(out, R_OK) == 0)
            return 1;
    }
    return 0;
}

/* Process a single file, writing output to 'out'. */
static int process_file(const char *path, vector_t *macros,
                        const char **search_paths, size_t num_paths,
                        strbuf_t *out)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror(path);
        return 0;
    }
    char *dir = NULL;
    const char *slash = strrchr(path, '/');
    if (slash) {
        size_t len = (size_t)(slash - path) + 1;
        dir = vc_alloc_or_exit(len + 1);
        memcpy(dir, path, len);
        dir[len] = '\0';
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    while ((n = getline(&line, &cap, fp)) != -1) {
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';

        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;

        if (strncmp(p, "#include", 8) == 0 && (p[8] == ' ' || p[8] == '\t')) {
            char *quote = strchr(p, '"');
            char *end = quote ? strchr(quote + 1, '"') : NULL;
            if (quote && end) {
                char name[256];
                size_t l = (size_t)(end - quote - 1);
                snprintf(name, sizeof(name), "%.*s", (int)l, quote + 1);
                char incpath[512];
                if (!find_include(name, dir, search_paths, num_paths, incpath, sizeof(incpath))) {
                    fprintf(stderr, "vc: include file '%s' not found\n", name);
                    free(line);
                    free(dir);
                    fclose(fp);
                    return 0;
                }
                if (!process_file(incpath, macros, search_paths, num_paths, out)) {
                    free(line);
                    free(dir);
                    fclose(fp);
                    return 0;
                }
            }
        } else if (strncmp(p, "#define", 7) == 0 && (p[7] == ' ' || p[7] == '\t')) {
            char *nptr = p + 7;
            while (*nptr == ' ' || *nptr == '\t')
                nptr++;
            char *val = nptr;
            while (*val && !isspace((unsigned char)*val))
                val++;
            if (*val) {
                *val++ = '\0';
                while (*val == ' ' || *val == '\t')
                    val++;
            } else {
                val = "";
            }
            if (!macro_define(macros, nptr, val)) {
                free(line);
                free(dir);
                fclose(fp);
                return 0;
            }
        } else {
            strbuf_t tmp;
            strbuf_init(&tmp);
            macro_expand_line(p, macros, &tmp);
            strbuf_append(&tmp, "\n");
            strbuf_append(out, tmp.data);
            strbuf_free(&tmp);
        }
    }
    free(line);
    free(dir);
    fclose(fp);
    return 1;
}

/* Entry point: preprocess the file and return a newly allocated buffer. */
char *preproc_run(const char *path,
                  const char **search_paths,
                  size_t num_paths)
{
    vector_t macros;
    vector_init(&macros, sizeof(macro_t));
    strbuf_t out;
    strbuf_init(&out);
    int ok = process_file(path, &macros, search_paths, num_paths, &out);
    for (size_t i = 0; i < macros.count; i++)
        macro_free(&((macro_t *)macros.data)[i]);
    vector_free(&macros);
    char *res = NULL;
    if (ok)
        res = vc_strdup(out.data ? out.data : "");
    strbuf_free(&out);
    return res;
}

