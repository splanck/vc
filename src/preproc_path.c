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

/* Default system include search paths */
#ifdef __linux__
#ifndef MULTIARCH_FALLBACK
#define MULTIARCH_FALLBACK "x86_64-linux-gnu"
#endif
#endif


#if !defined(GCC_INCLUDE_DIR)
static char *gcc_include_cached = NULL;
static int gcc_include_initialized = 0;
#endif

static char *multiarch_cached = NULL;
static int multiarch_initialized = 0;
static int std_dirs_initialized = 0;

static const char *get_multiarch_dir(void)
{
#if defined(__linux__)
    if (!multiarch_initialized) {
        multiarch_initialized = 1;
        FILE *fp = popen("gcc -print-multiarch 2>/dev/null", "r");
        if (fp) {
            char buf[256];
            if (fgets(buf, sizeof(buf), fp)) {
                size_t len = strlen(buf);
                while (len && (buf[len-1] == '\n' || buf[len-1] == '\r'))
                    buf[--len] = '\0';
                if (len)
                    multiarch_cached = vc_strndup(buf, len);
            }
            pclose(fp);
        }
        if (!multiarch_cached) {
#ifdef MULTIARCH
            multiarch_cached = vc_strdup(MULTIARCH);
#else
            multiarch_cached = vc_strdup(MULTIARCH_FALLBACK);
#endif
        }
    }
    return multiarch_cached;
#else
    return NULL;
#endif
}

static const char *get_gcc_include_dir(void)
{
#ifdef GCC_INCLUDE_DIR
    return GCC_INCLUDE_DIR;
#else
    if (!gcc_include_initialized) {
        gcc_include_initialized = 1;
        FILE *fp = popen("gcc -print-file-name=include 2>/dev/null", "r");
        if (fp) {
            char buf[4096];
            if (fgets(buf, sizeof(buf), fp)) {
                size_t len = strlen(buf);
                while (len && (buf[len-1] == '\n' || buf[len-1] == '\r'))
                    buf[--len] = '\0';
                if (len)
                    gcc_include_cached = vc_strndup(buf, len);
            }
            pclose(fp);
        }
        if (!gcc_include_cached) {
#if defined(__linux__)
            const char *multi = get_multiarch_dir();
            size_t len = strlen("/usr/lib/gcc/") + strlen(multi) + strlen("/include");
            gcc_include_cached = vc_alloc_or_exit(len + 1);
            snprintf(gcc_include_cached, len + 1, "/usr/lib/gcc/%s/include", multi);
#else
            gcc_include_cached = vc_strdup("/usr/lib/gcc/include");
#endif
        }
    }
    return gcc_include_cached;
#endif
}

static void init_std_include_dirs(void);

static const char *std_include_dirs[] = {
#if defined(__linux__)
    NULL, /* /usr/include/<multiarch> */
    NULL, /* gcc include */
#elif defined(__NetBSD__) || defined(__FreeBSD__)
    NULL, /* gcc include */
#endif
    "/usr/local/include",
    "/usr/include",
    NULL
};

static void init_std_include_dirs(void)
{
    if (std_dirs_initialized)
        return;
#if defined(__linux__)
    const char *multi = get_multiarch_dir();
    size_t len = strlen("/usr/include/") + strlen(multi);
    char *path = vc_alloc_or_exit(len + 1);
    snprintf(path, len + 1, "/usr/include/%s", multi);
    std_include_dirs[0] = path;
    std_include_dirs[1] = get_gcc_include_dir();
#elif defined(__NetBSD__) || defined(__FreeBSD__)
    std_include_dirs[0] = get_gcc_include_dir();
#endif
    std_dirs_initialized = 1;
}

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
    init_std_include_dirs();
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
                         const char *sysroot)
{
    init_std_include_dirs();
    const char *gcc_dir = get_gcc_include_dir();
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

    if (sysroot && *sysroot) {
        for (size_t i = 0; std_include_dirs[i]; i++) {
            const char *base = std_include_dirs[i];
            size_t root_len = strlen(sysroot);
            while (root_len && (sysroot[root_len - 1] == '/' ||
                                sysroot[root_len - 1] == '\\'))
                root_len--; /* strip trailing slash */
            size_t len = root_len + strlen(base);
            char *dup = malloc(len + 1);
            if (!dup) {
                free_string_vector(search_dirs);
                return 0;
            }
            memcpy(dup, sysroot, root_len);
            dup[root_len] = '\0';
            strcat(dup, base);
            if (!vector_push(search_dirs, &dup)) {
                free(dup);
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
    init_std_include_dirs();
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
    for (size_t i = builtin_start; std_include_dirs[i]; i++)
        fprintf(fp, "  %s\n", std_include_dirs[i]);
}

void preproc_path_cleanup(void)
{
#ifndef GCC_INCLUDE_DIR
    free(gcc_include_cached);
    gcc_include_cached = NULL;
    gcc_include_initialized = 0;
#endif
#if defined(__linux__)
    free((char *)std_include_dirs[0]);
    std_include_dirs[0] = NULL;
    std_include_dirs[1] = NULL;
    free(multiarch_cached);
    multiarch_cached = NULL;
    multiarch_initialized = 0;
#elif defined(__NetBSD__) || defined(__FreeBSD__)
    std_include_dirs[0] = NULL;
#endif
    std_dirs_initialized = 0;
}

