#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "util.h"
#include "preproc_path.h"
#include "include_path_cache.h"
#include <stdbool.h>

/* Default system include search paths */
#ifdef __linux__
#ifndef MULTIARCH_FALLBACK
#define MULTIARCH_FALLBACK "x86_64-linux-gnu"
#endif
#endif


static vector_t extra_sys_dirs;
static int verbose_includes = 0;
static const char *internal_libc_dir = PROJECT_ROOT "/libc/include";

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
    include_path_cache_init();
    const char * const *std_include_dirs = include_path_cache_std_dirs();
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

    if (max_len == SIZE_MAX) {
        fprintf(stderr, "vc: include path too long\n");
        return NULL;
    }
    max_len += 1;
    char *out_path = vc_alloc_or_exit(max_len);
    if (endc == '"' && dir && start == 0) {
        snprintf(out_path, max_len, "%s%s", dir, fname);
        if (verbose_includes)
            fprintf(stderr, "checking %s%s\n", dir, fname);
        if (access(out_path, R_OK) == 0) {
            if (out_idx)
                *out_idx = (size_t)-1;
            if (verbose_includes)
                fprintf(stderr, "found %s\n", out_path);
            return out_path;
        }
    }
    for (size_t i = start; i < incdirs->count; i++) {
        const char *base = ((const char **)incdirs->data)[i];
        snprintf(out_path, max_len, "%s/%s", base, fname);
        if (verbose_includes)
            fprintf(stderr, "checking %s/%s\n", base, fname);
        if (access(out_path, R_OK) == 0) {
            if (out_idx)
                *out_idx = i;
            if (verbose_includes)
                fprintf(stderr, "found %s\n", out_path);
            return out_path;
        }
    }
    size_t builtin_start = 0;
    if (start > incdirs->count)
        builtin_start = start - incdirs->count;
    if (endc == '<') {
        for (size_t i = builtin_start; i < extra_sys_dirs.count; i++) {
            const char *base = ((const char **)extra_sys_dirs.data)[i];
            snprintf(out_path, max_len, "%s/%s", base, fname);
            if (verbose_includes)
                fprintf(stderr, "checking %s/%s\n", base, fname);
            if (access(out_path, R_OK) == 0) {
                if (out_idx)
                    *out_idx = incdirs->count + i;
                if (verbose_includes)
                    fprintf(stderr, "found %s\n", out_path);
                return out_path;
            }
        }
        size_t off = builtin_start > extra_sys_dirs.count ?
                      builtin_start - extra_sys_dirs.count : 0;
        for (size_t i = off; std_include_dirs[i]; i++) {
            snprintf(out_path, max_len, "%s/%s", std_include_dirs[i], fname);
            if (verbose_includes)
                fprintf(stderr, "checking %s/%s\n", std_include_dirs[i], fname);
            if (access(out_path, R_OK) == 0) {
                if (out_idx)
                    *out_idx = incdirs->count + extra_sys_dirs.count + i;
                if (verbose_includes)
                    fprintf(stderr, "found %s\n", out_path);
                return out_path;
            }
        }
        free(out_path);
        return NULL;
    }
    snprintf(out_path, max_len, "%s", fname);
    if (verbose_includes)
        fprintf(stderr, "checking %s\n", out_path);
    if (access(out_path, R_OK) == 0) {
        if (out_idx)
            *out_idx = (size_t)-1;
        if (verbose_includes)
            fprintf(stderr, "found %s\n", out_path);
        return out_path;
    }
    for (size_t i = builtin_start; i < extra_sys_dirs.count; i++) {
        const char *base = ((const char **)extra_sys_dirs.data)[i];
        snprintf(out_path, max_len, "%s/%s", base, fname);
        if (verbose_includes)
            fprintf(stderr, "checking %s/%s\n", base, fname);
        if (access(out_path, R_OK) == 0) {
            if (out_idx)
                *out_idx = incdirs->count + i;
            if (verbose_includes)
                fprintf(stderr, "found %s\n", out_path);
            return out_path;
        }
    }
    size_t off = builtin_start > extra_sys_dirs.count ?
                  builtin_start - extra_sys_dirs.count : 0;
    for (size_t i = off; std_include_dirs[i]; i++) {
        snprintf(out_path, max_len, "%s/%s", std_include_dirs[i], fname);
        if (verbose_includes)
            fprintf(stderr, "checking %s/%s\n", std_include_dirs[i], fname);
        if (access(out_path, R_OK) == 0) {
            if (out_idx)
                *out_idx = incdirs->count + extra_sys_dirs.count + i;
            if (verbose_includes)
                fprintf(stderr, "found %s\n", out_path);
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
    size_t start = search_dirs->count;
    char *tok, *sp;
#if defined(_WIN32)
    const char *sep = ";:";
#else
    const char *sep = ":";
#endif
    tok = strtok_r(tmp, sep, &sp);
    while (tok) {
        if (*tok) {
            char *dup = vc_strdup(tok);
            if (!dup || !vector_push(search_dirs, &dup)) {
                free(dup);
                for (size_t i = start; i < search_dirs->count; i++)
                    free(((char **)search_dirs->data)[i]);
                search_dirs->count = start;
                free(tmp);
                return 0;
            }
        }
        tok = strtok_r(NULL, sep, &sp);
    }
    free(tmp);
    return 1;
}

int collect_include_dirs(vector_t *search_dirs,
                         const vector_t *include_dirs,
                         const char *sysroot,
                         const char *vc_sysinclude,
                         bool internal_libc)
{
    include_path_cache_init();
    const char *gcc_dir = include_path_cache_gcc_dir();
    const char * const *std_include_dirs = include_path_cache_std_dirs();
    assert(strcspn(gcc_dir, " \t\n") == strlen(gcc_dir));
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
    if (!append_env_paths(getenv("CPATH"), search_dirs)) {
        free_string_vector(search_dirs);
        return 0;
    }
    if (!append_env_paths(getenv("C_INCLUDE_PATH"), search_dirs)) {
        free_string_vector(search_dirs);
        return 0;
    }

    free_string_vector(&extra_sys_dirs);
    vector_init(&extra_sys_dirs, sizeof(char *));
    const char *sysinc = vc_sysinclude && *vc_sysinclude ? vc_sysinclude
                       : getenv("VC_SYSINCLUDE");
    if (sysinc && *sysinc) {
        if (!append_env_paths(sysinc, &extra_sys_dirs)) {
            free_string_vector(search_dirs);
            free_string_vector(&extra_sys_dirs);
            return 0;
        }
    }
    if (internal_libc) {
        int exists = 0;
        for (size_t i = 0; i < extra_sys_dirs.count; i++) {
            const char *p = ((const char **)extra_sys_dirs.data)[i];
            if (strcmp(p, internal_libc_dir) == 0) {
                exists = 1;
                break;
            }
        }
        if (!exists) {
            char *dup = vc_strdup(internal_libc_dir);
            if (!dup || !vector_push(&extra_sys_dirs, &dup)) {
                free(dup);
                free_string_vector(search_dirs);
                free_string_vector(&extra_sys_dirs);
                return 0;
            }
        }
    }

    if (sysroot && *sysroot) {
        vector_t *dest = internal_libc ? &extra_sys_dirs : search_dirs;
        for (size_t i = 0; std_include_dirs[i]; i++) {
            const char *base = std_include_dirs[i];
            size_t root_len = strlen(sysroot);
            while (root_len && (sysroot[root_len - 1] == '/' ||
                                sysroot[root_len - 1] == '\\'))
                root_len--; /* strip trailing slash */
            size_t len = root_len + strlen(base);
            char *dup = malloc(len + 1);
            if (!dup) {
                free_string_vector(dest);
                if (dest == search_dirs)
                    free_string_vector(&extra_sys_dirs);
                else
                    free_string_vector(search_dirs);
                return 0;
            }
            memcpy(dup, sysroot, root_len);
            dup[root_len] = '\0';
            strcat(dup, base);
            if (!vector_push(dest, &dup)) {
                free(dup);
                free_string_vector(dest);
                if (dest == search_dirs)
                    free_string_vector(&extra_sys_dirs);
                else
                    free_string_vector(search_dirs);
                return 0;
            }
        }
    }
    return 1;
}

void print_include_search_dirs(FILE *fp, char endc, const char *dir,
                               const vector_t *incdirs, size_t start)
{
    include_path_cache_init();
    const char * const *std_include_dirs = include_path_cache_std_dirs();
    if (endc == '"' && dir && start == 0)
        fprintf(fp, "  %s\n", dir);
    for (size_t i = start; i < incdirs->count; i++) {
        const char *base = ((const char **)incdirs->data)[i];
        fprintf(fp, "  %s\n", base);
    }
    size_t builtin_start = 0;
    if (start > incdirs->count)
        builtin_start = start - incdirs->count;
    if (endc != '<')
        fprintf(fp, "  .\n");
    for (size_t i = builtin_start; i < extra_sys_dirs.count; i++)
        fprintf(fp, "  %s\n", ((char **)extra_sys_dirs.data)[i]);
    size_t off = builtin_start > extra_sys_dirs.count ?
                  builtin_start - extra_sys_dirs.count : 0;
    for (size_t i = off; std_include_dirs[i]; i++)
        fprintf(fp, "  %s\n", std_include_dirs[i]);
}

void preproc_set_verbose_includes(bool flag)
{
    verbose_includes = flag ? 1 : 0;
}

void preproc_set_internal_libc_dir(const char *path)
{
    internal_libc_dir = (path && *path) ? path : PROJECT_ROOT "/libc/include";
}

void preproc_path_cleanup(void)
{
    include_path_cache_cleanup();
    free_string_vector(&extra_sys_dirs);
}

