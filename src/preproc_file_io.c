#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "preproc_file_io.h"
#include "preproc_macros.h"
#include "preproc_path.h"

int include_stack_contains(vector_t *stack, const char *path)
{
    char *canon = realpath(path, NULL);
    if (!canon) {
        if (errno != ENOENT)
            perror(path);
        return 0;
    }
    for (size_t i = 0; i < stack->count; i++) {
        const include_entry_t *e = &((include_entry_t *)stack->data)[i];
        if (strcmp(e->path, canon) == 0) {
            free(canon);
            return 1;
        }
    }
    free(canon);
    return 0;
}

int include_stack_push(vector_t *stack, const char *path, size_t idx)
{
    char *canon = realpath(path, NULL);
    if (!canon) {
        canon = vc_strdup(path);
        if (!canon) {
            vc_oom();
            return 0;
        }
    }
    include_entry_t ent = { canon, idx };
    if (!vector_push(stack, &ent)) {
        free(canon);
        vc_oom();
        return 0;
    }
    return 1;
}

void include_stack_pop(vector_t *stack)
{
    if (stack->count) {
        include_entry_t *e = &((include_entry_t *)stack->data)[stack->count - 1];
        free(e->path);
        stack->count--;
    }
}

static char *read_file_lines_internal(const char *path, char ***out_lines)
{
    char *text = vc_read_file(path);
    if (!text)
        return NULL;

    size_t len = strlen(text);

    size_t w = 0;
    for (size_t r = 0; r < len; r++) {
        if (text[r] == '\\' && r + 1 < len && text[r + 1] == '\n') {
            r++;
            continue;
        }
        text[w++] = text[r];
    }
    text[w] = '\0';
    len = w;

    size_t line_count = 1;
    for (char *p = text; *p; p++)
        if (*p == '\n')
            line_count++;
    if (len > 0 && text[len - 1] == '\n')
        line_count--;

    char **lines = vc_alloc_or_exit(sizeof(char *) * (line_count + 1));

    size_t idx = 0;
    lines[idx++] = text;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n') {
            text[i] = '\0';
            if (i + 1 < len)
                lines[idx++] = &text[i + 1];
        }
    }
    lines[idx] = NULL;
    *out_lines = lines;
    return text;
}

char *read_file_lines(const char *path, char ***out_lines)
{
    return read_file_lines_internal(path, out_lines);
}

int load_file_lines(const char *path, char ***out_lines,
                    char **out_dir, char **out_text)
{
    char **lines;
    char *text = read_file_lines_internal(path, &lines);
    if (!text)
        return 0;

    char *dir = NULL;
    const char *slash = strrchr(path, '/');
    if (slash) {
        size_t len = (size_t)(slash - path) + 1;
        dir = vc_strndup(path, len);
        if (!dir) {
            free(lines);
            free(text);
            return 0;
        }
    }

    *out_lines = lines;
    *out_dir = dir;
    *out_text = text;
    return 1;
}

int load_and_register_file(const char *path, vector_t *stack, size_t idx,
                           char ***out_lines, char **out_dir, char **out_text,
                           preproc_context_t *ctx)
{
    if (!load_file_lines(path, out_lines, out_dir, out_text))
        return 0;

    if (!include_stack_push(stack, path, idx)) {
        free(*out_lines);
        free(*out_text);
        free(*out_dir);
        return 0;
    }

    if (!record_dependency(ctx, path)) {
        free(*out_lines);
        free(*out_text);
        free(*out_dir);
        include_stack_pop(stack);
        return 0;
    }

    return 1;
}

int process_all_lines(char **lines, const char *path, const char *dir,
                      vector_t *macros, vector_t *conds, strbuf_t *out,
                      const vector_t *incdirs, vector_t *stack,
                      preproc_context_t *ctx)
{
    extern int process_line(char *, const char *, vector_t *, vector_t *,
                            strbuf_t *, const vector_t *, vector_t *,
                            preproc_context_t *);
    for (size_t i = 0; lines[i]; i++) {
        preproc_set_location(path, i + 1, 1);
        if (!process_line(lines[i], dir, macros, conds, out, incdirs, stack,
                          ctx))
            return 0;
    }
    return 1;
}

void cleanup_file_resources(char *text, char **lines, char *dir)
{
    free(lines);
    free(text);
    free(dir);
}

