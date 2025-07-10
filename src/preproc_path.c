#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "preproc_path.h"

/* Default system include search paths */
#ifdef __linux__
#ifndef MULTIARCH
#define MULTIARCH "x86_64-linux-gnu"
#endif
#endif

static const char *std_include_dirs[] = {
#ifdef __linux__
    "/usr/include/" MULTIARCH,
#endif
    "/usr/local/include",
    "/usr/include",
    NULL
};

int record_dependency(preproc_context_t *ctx, const char *path)
{
    char *canon = realpath(path, NULL);
    if (!canon)
        canon = vc_strdup(path);
    if (!canon)
        return 0;

    for (size_t i = 0; i < ctx->deps.count; i++) {
        const char *p = ((const char **)ctx->deps.data)[i];
        if (strcmp(p, canon) == 0) {
            free(canon);
            return 1;
        }
    }

    if (!vector_push(&ctx->deps, &canon)) {
        free(canon);
        vc_oom();
        return 0;
    }

    return 1;
}


int pragma_once_contains(preproc_context_t *ctx, const char *path)
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

int pragma_once_add(preproc_context_t *ctx, const char *path)
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
    if (!vector_push(&ctx->pragma_once_files, &canon)) {
        free(canon);
        vc_oom();
        return 0;
    }
    return 1;
}

char *find_include_path(const char *fname, char endc, const char *dir,
                        const vector_t *incdirs, size_t start, size_t *out_idx)
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


int append_env_paths(const char *env, vector_t *search_dirs)
{
    if (!env || !*env)
        return 1;

    char *tmp = vc_strdup(env);
    if (!tmp)
        return 0;
    char *tok, *sp;
    tok = strtok_r(tmp, ":", &sp);
    while (tok) {
        if (*tok) {
            char *dup = vc_strdup(tok);
            if (!dup || !vector_push(search_dirs, &dup)) {
                free(dup);
                free(tmp);
                return 0;
            }
        }
        tok = strtok_r(NULL, ":", &sp);
    }
    free(tmp);
    return 1;
}

int collect_include_dirs(vector_t *search_dirs,
                         const vector_t *include_dirs)
{
    vector_init(search_dirs, sizeof(char *));
    for (size_t i = 0; i < include_dirs->count; i++) {
        const char *s = ((const char **)include_dirs->data)[i];
        char *dup = vc_strdup(s);
        if (!dup || !vector_push(search_dirs, &dup)) {
            free(dup);
            free_string_vector(search_dirs);
            return 0;
        }
    }

    if (!append_env_paths(getenv("VCPATH"), search_dirs)) {
        free_string_vector(search_dirs);
        return 0;
    }
    if (!append_env_paths(getenv("VCINC"), search_dirs)) {
        free_string_vector(search_dirs);
        return 0;
    }
    return 1;
}

