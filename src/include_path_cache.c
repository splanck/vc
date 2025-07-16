#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "include_path_cache.h"
#include "util.h"

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
static int gcc_query_failed = 0;
static int sys_header_warned = 0;

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

static const char *get_multiarch_dir(void)
{
#if defined(__linux__)
    if (!multiarch_initialized) {
        multiarch_initialized = 1;
        FILE *fp = popen("gcc -print-multiarch 2>/dev/null", "r");
        if (!fp) {
            perror("popen");
            gcc_query_failed = 1;
        } else {
            char buf[256];
            if (fgets(buf, sizeof(buf), fp)) {
                size_t len = strlen(buf);
                while (len && (buf[len-1] == '\n' || buf[len-1] == '\r'))
                    buf[--len] = '\0';
                if (len)
                    multiarch_cached = vc_strndup(buf, len);
            } else {
                perror("fgets");
                gcc_query_failed = 1;
            }
            if (pclose(fp) == -1) {
                perror("pclose");
                gcc_query_failed = 1;
            }
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
        if (!fp) {
            perror("popen");
            gcc_query_failed = 1;
        } else {
            char buf[4096];
            if (fgets(buf, sizeof(buf), fp)) {
                size_t len = strlen(buf);
                while (len && (buf[len-1] == '\n' || buf[len-1] == '\r'))
                    buf[--len] = '\0';
                if (len)
                    gcc_include_cached = vc_strndup(buf, len);
            } else {
                perror("fgets");
                gcc_query_failed = 1;
            }
            if (pclose(fp) == -1) {
                perror("pclose");
                gcc_query_failed = 1;
            }
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
        if (gcc_query_failed && !sys_header_warned) {
            fprintf(stderr,
                    "vc: system headers could not be located. Use --vc-sysinclude=<dir> or VC_SYSINCLUDE\n");
            sys_header_warned = 1;
        }
    }
    return gcc_include_cached;
#endif
}

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

void include_path_cache_init(void)
{
    init_std_include_dirs();
}

const char *include_path_cache_multiarch(void)
{
    return get_multiarch_dir();
}

const char *include_path_cache_gcc_dir(void)
{
    init_std_include_dirs();
    return get_gcc_include_dir();
}

const char * const *include_path_cache_std_dirs(void)
{
    init_std_include_dirs();
    return std_include_dirs;
}

void include_path_cache_cleanup(void)
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
    gcc_query_failed = 0;
    sys_header_warned = 0;
}
