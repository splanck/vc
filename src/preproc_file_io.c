#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "util.h"
#include "preproc_file_io.h"
#include "preproc_macros.h"
#include "preproc_path.h"
#include "preproc_file.h"
#include "preproc_builtin.h"

static char *canonical_path(const char *path)
{
    char *canon = realpath(path, NULL);
    if (!canon)
        canon = vc_strdup(path);
    return canon;
}

/* per-context line tracking */

void line_state_push(preproc_context_t *ctx, const char *file, long delta,
                     char **prev_file, long *prev_delta)
{
    *prev_file = ctx->current_file;
    *prev_delta = ctx->line_delta;
    ctx->current_file = vc_strdup(file);
    ctx->line_delta = delta;
}

void line_state_pop(preproc_context_t *ctx, char *prev_file, long prev_delta)
{
    free(ctx->current_file);
    ctx->current_file = prev_file;
    ctx->line_delta = prev_delta;
}

void preproc_apply_line_directive(preproc_context_t *ctx,
                                  const char *file, int line)
{
    if (file) {
        free(ctx->current_file);
        ctx->current_file = vc_strdup(file);
    }
    ctx->line_delta = line - ((long)preproc_get_line(ctx) + 1);
    preproc_set_location(ctx, ctx->current_file, (size_t)line - 1, 1);
}

int include_stack_contains(vector_t *stack, const char *path)
{
    char *canon = canonical_path(path);
    if (!canon)
        return 0;
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

int include_stack_push(vector_t *stack, const char *path, size_t idx,
                       preproc_context_t *ctx)
{
    char *canon = canonical_path(path);
    if (!canon) {
        vc_oom();
        return 0;
    }
    include_entry_t ent = { canon, idx, ctx->system_header };
    if (!vector_push(stack, &ent)) {
        free(canon);
        vc_oom();
        return 0;
    }
    if (stack->count == 1)
        preproc_set_base_file(ctx, canon);
    preproc_set_include_level(ctx, stack->count - 1);
    return 1;
}

void include_stack_pop(vector_t *stack, preproc_context_t *ctx)
{
    if (stack->count) {
        include_entry_t *e = &((include_entry_t *)stack->data)[stack->count - 1];
        ctx->system_header = e->prev_system_header;
        free(e->path);
        stack->count--;
    }
    if (stack->count)
        preproc_set_include_level(ctx, stack->count - 1);
    else
        preproc_set_include_level(ctx, 0);
}

static char *read_file_lines_internal(const char *path, char ***out_lines)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return NULL;
    }

    size_t cap = 8192;
    size_t len = 0;
    char *text = vc_alloc_or_exit(cap);

    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\\') {
            int next = fgetc(f);
            if (next == '\n')
                continue;
            if (next == '\r') {
                int next2 = fgetc(f);
                if (next2 == '\n')
                    continue;
                if (next2 != EOF)
                    ungetc(next2, f);
            }
            if (next != EOF)
                ungetc(next, f);
        } else if (c == '\r') {
            continue;
        }

        if (len + 1 >= cap) {
            if (cap > SIZE_MAX / 2)
                cap = cap + 1;
            else
                cap *= 2;
            text = vc_realloc_or_exit(text, cap);
        }
        text[len++] = (char)c;
    }

    if (ferror(f)) {
        perror(path);
        fclose(f);
        free(text);
        return NULL;
    }

    fclose(f);

    text[len] = '\0';

    size_t line_count = 1;
    for (size_t i = 0; i < len; i++)
        if (text[i] == '\n')
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

    if (!include_stack_push(stack, path, idx, ctx)) {
        free(*out_lines);
        free(*out_text);
        free(*out_dir);
        return 0;
    }

    if (!record_dependency(ctx, path)) {
        free(*out_lines);
        free(*out_text);
        free(*out_dir);
        include_stack_pop(stack, ctx);
        return 0;
    }

    return 1;
}

int process_all_lines(char **lines, const char *path, const char *dir,
                      vector_t *macros, vector_t *conds, strbuf_t *out,
                      const vector_t *incdirs, vector_t *stack,
                      preproc_context_t *ctx)
{
    /* defined in preproc_directives.c */
    for (size_t i = 0; lines[i]; i++) {
        long line_tmp = (long)(i + 1) + ctx->line_delta;
        if (line_tmp < 0)
            line_tmp = 0;
        size_t line = (size_t)line_tmp;
        preproc_set_location(ctx,
                             ctx->current_file ? ctx->current_file : path,
                             line, 1);
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

