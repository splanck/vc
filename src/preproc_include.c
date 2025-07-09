#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "util.h"
#include "preproc_include.h"

/* Default system include search paths */
static const char *std_include_dirs[] = {
    "/usr/local/include",
    "/usr/include",
    NULL
};

/* Return 1 if all conditional states on the stack are active */
static int stack_active(vector_t *conds)
{
    for (size_t i = 0; i < conds->count; i++) {
        cond_state_t *c = &((cond_state_t *)conds->data)[i];
        if (!c->taking)
            return 0;
    }
    return 1;
}

/* Wrapper used by directive handlers */
static int is_active(vector_t *conds)
{
    return stack_active(conds);
}

/* Return non-zero when the include stack already contains PATH. */
static int include_stack_contains(vector_t *stack, const char *path)
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

/* Return non-zero when the pragma_once list contains PATH */
static int pragma_once_contains(preproc_context_t *ctx, const char *path)
{
    char *canon = realpath(path, NULL);
    if (!canon)
        canon = vc_strdup(path);
    if (!canon)
        return 0;
    for (size_t i = 0; i < ctx->pragma_once_files.count; i++) {
        const char *p = ((const char **)ctx->pragma_once_files.data)[i];
        if (strcmp(p, canon) == 0) {
            free(canon);
            return 1;
        }
    }
    free(canon);
    return 0;
}

/* Locate the full path of an include file */
static char *find_include_path(const char *fname, char endc, const char *dir,
                               const vector_t *incdirs, size_t start,
                               size_t *out_idx)
{
    size_t fname_len = strlen(fname);
    size_t max_len = fname_len;
    if (endc == '"' && dir && start == 0) {
        size_t len = strlen(dir) + fname_len;
        if (len > max_len)
            max_len = len;
    }
    for (size_t i = 0; i < incdirs->count; i++) {
        const char *base = ((const char **)incdirs->data)[i];
        size_t len = strlen(base) + 1 + fname_len;
        if (len > max_len)
            max_len = len;
    }
    for (size_t i = 0; std_include_dirs[i]; i++) {
        size_t len = strlen(std_include_dirs[i]) + 1 + fname_len;
        if (len > max_len)
            max_len = len;
    }
    char *out_path = vc_alloc_or_exit(max_len + 1);
    if (endc == '"' && dir && start == 0) {
        snprintf(out_path, max_len + 1, "%s%s", dir, fname);
        if (access(out_path, R_OK) == 0) {
            if (out_idx)
                *out_idx = (size_t)-1;
            return out_path;
        }
    }
    for (size_t i = start; i < incdirs->count; i++) {
        const char *base = ((const char **)incdirs->data)[i];
        snprintf(out_path, max_len + 1, "%s/%s", base, fname);
        if (access(out_path, R_OK) == 0) {
            if (out_idx)
                *out_idx = i;
            return out_path;
        }
    }
    size_t builtin_start = 0;
    if (start > incdirs->count)
        builtin_start = start - incdirs->count;
    if (endc == '<') {
        for (size_t i = builtin_start; std_include_dirs[i]; i++) {
            snprintf(out_path, max_len + 1, "%s/%s", std_include_dirs[i], fname);
            if (access(out_path, R_OK) == 0) {
                if (out_idx)
                    *out_idx = incdirs->count + i;
                return out_path;
            }
        }
        free(out_path);
        return NULL;
    }
    snprintf(out_path, max_len + 1, "%s", fname);
    if (access(out_path, R_OK) == 0) {
        if (out_idx)
            *out_idx = (size_t)-1;
        return out_path;
    }
    for (size_t i = builtin_start; std_include_dirs[i]; i++) {
        snprintf(out_path, max_len + 1, "%s/%s", std_include_dirs[i], fname);
        if (access(out_path, R_OK) == 0) {
            if (out_idx)
                *out_idx = incdirs->count + i;
            return out_path;
        }
    }
    free(out_path);
    return NULL;
}

/* Parse the include filename from LINE. ENDC receives '"' or '>' */
static char *parse_include_name(char *line, char *endc)
{
    char *start = strchr(line, '"');
    *endc = '"';
    if (!start) {
        start = strchr(line, '<');
        *endc = '>';
    }
    if (!start)
        return NULL;
    char *end = strchr(start + 1, *endc);
    if (!end)
        return NULL;
    size_t len = (size_t)(end - start - 1);
    return vc_strndup(start + 1, len);
}

/* Shared logic for processing an include file */
static int process_include_file(const char *fname, const char *chosen,
                                size_t idx, vector_t *macros, vector_t *conds,
                                strbuf_t *out, const vector_t *incdirs,
                                vector_t *stack, preproc_context_t *ctx)
{
    vector_t subconds;
    vector_init(&subconds, sizeof(cond_state_t));
    int ok = 1;
    if (is_active(conds)) {
        if (!chosen) {
            errno = ENOENT;
            perror(fname);
            ok = 0;
        } else if (!pragma_once_contains(ctx, chosen)) {
            if (include_stack_contains(stack, chosen)) {
                fprintf(stderr, "Include cycle detected: %s\n", chosen);
                ok = 0;
            } else if (!process_file(chosen, macros, &subconds, out,
                                     incdirs, stack, ctx, idx)) {
                if (errno)
                    perror(chosen);
                ok = 0;
            }
        }
    }
    vector_free(&subconds);
    return ok;
}

/* external declaration from preproc_file.c */
int process_file(const char *path, vector_t *macros, vector_t *conds,
                 strbuf_t *out, const vector_t *incdirs, vector_t *stack,
                 preproc_context_t *ctx, size_t idx);

int handle_include(char *line, const char *dir, vector_t *macros,
                   vector_t *conds, strbuf_t *out,
                   const vector_t *incdirs, vector_t *stack,
                   preproc_context_t *ctx)
{
    char endc;
    char *fname = parse_include_name(line, &endc);
    int result = 1;
    if (fname) {
        size_t idx = SIZE_MAX;
        char *incpath = find_include_path(fname, endc, dir, incdirs, 0, &idx);
        if (!process_include_file(fname, incpath, idx, macros, conds, out,
                                  incdirs, stack, ctx))
            result = 0;
        free(incpath);
    }
    free(fname);
    return result;
}

int handle_include_next(char *line, const char *dir, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs, vector_t *stack,
                        preproc_context_t *ctx)
{
    (void)dir;
    char endc;
    char *fname = parse_include_name(line, &endc);
    int result = 1;
    if (fname) {
        size_t cur = SIZE_MAX;
        if (stack->count) {
            const include_entry_t *e =
                &((include_entry_t *)stack->data)[stack->count - 1];
            cur = e->dir_index;
        }
        size_t idx = SIZE_MAX;
        size_t start_idx = (cur == (size_t)-1) ? 0 : cur + 1;
        char *incpath = find_include_path(fname, '>', NULL, incdirs,
                                          start_idx, &idx);
        if (!process_include_file(fname, incpath, idx, macros, conds, out,
                                  incdirs, stack, ctx))
            result = 0;
        free(incpath);
    }
    free(fname);
    return result;
}

