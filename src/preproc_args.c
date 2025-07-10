#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "preproc_args.h"
#include "strbuf.h"
#include "util.h"
#include "vector.h"

/* Duplicate an argument substring trimming surrounding whitespace. */
static char *dup_arg_segment(const char *line, size_t start, size_t end)
{
    while (start < end && (line[start] == ' ' || line[start] == '\t'))
        start++;
    while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t'))
        end--;
    return vc_strndup(line + start, end - start);
}

/* Scan for the next argument delimiter updating nesting level. */
static char find_arg_delim(const char *line, size_t *p, int *nest,
                           size_t *out_end)
{
    for (;; (*p)++) {
        char c = line[*p];
        if (c == '\0')
            return 0;
        if (c == '(') {
            (*nest)++;
            continue;
        }
        if (c == ')') {
            if (*nest > 0) {
                (*nest)--;
                continue;
            }
            *out_end = *p;
            return ')';
        }
        if (c == ',' && *nest == 0) {
            *out_end = *p;
            return ',';
        }
    }
}

int parse_macro_args(const char *line, size_t *pos, vector_t *out)
{
    size_t p = *pos;
    vector_init(out, sizeof(char *));
    while (line[p] == ' ' || line[p] == '\t')
        p++;
    if (line[p] != '(')
        return 0;
    p++;
    while (line[p] == ' ' || line[p] == '\t')
        p++;
    if (line[p] == ')') {
        *pos = p + 1;
        return 1;
    }

    size_t arg_start = p;
    int nest = 0;
    for (;;) {
        size_t end;
        char delim = find_arg_delim(line, &p, &nest, &end);
        if (!delim)
            break;
        char *dup = dup_arg_segment(line, arg_start, end);
        if (!dup || !vector_push(out, &dup)) {
            free(dup);
            goto fail;
        }
        if (delim == ')') {
            *pos = end + 1;
            return 1;
        }
        p++;
        while (line[p] == ' ' || line[p] == '\t')
            p++;
        arg_start = p;
    }
fail:
    for (size_t i = 0; i < out->count; i++)
        free(((char **)out->data)[i]);
    vector_free(out);
    return 0;
}

int gather_varargs(vector_t *args, size_t fixed,
                   char ***out_ap, char **out_va)
{
    strbuf_t sb;
    strbuf_init(&sb);
    for (size_t i = fixed; i < args->count; i++) {
        if (i > fixed)
            strbuf_append(&sb, ",");
        strbuf_append(&sb, ((char **)args->data)[i]);
    }
    char *va = vc_strdup(sb.data ? sb.data : "");
    strbuf_free(&sb);
    char **ap = malloc((fixed + 1) * sizeof(char *));
    if (!ap) {
        vc_oom();
        free(va);
        return 0;
    }
    for (size_t i = 0; i < fixed; i++)
        ap[i] = ((char **)args->data)[i];
    ap[fixed] = va;
    *out_ap = ap;
    *out_va = va;
    return 1;
}

void free_arg_vector(vector_t *v)
{
    for (size_t i = 0; i < v->count; i++)
        free(((char **)v->data)[i]);
    vector_free(v);
}

