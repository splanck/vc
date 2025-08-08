#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>
#include <ctype.h>

#include "util.h"
#include "preproc_include.h"
#include "preproc_cond.h"
#include "preproc_file_io.h"
#include "preproc_path.h"
#include "preproc_file.h"
#include "preproc_macros.h"
#include "preproc_utils.h"


/* Return non-zero when the include stack already contains PATH. */
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
    char *p = end + 1;
    while (isspace((unsigned char)*p))
        p++;
    if (*p && !(p[0] == '/' && (p[1] == '*' || p[1] == '/')))
        return NULL;
    size_t len = (size_t)(end - start - 1);
    return vc_strndup(start + 1, len);
}

static void report_missing_include(const char *line, const char *fname,
                                   char endc, const char *dir,
                                   const vector_t *incdirs, size_t start)
{
    errno = ENOENT;
    perror(fname);
    fprintf(stderr, "%s\n", line);
    fprintf(stderr, "Searched directories:\n");
    print_include_search_dirs(stderr, endc, dir, incdirs, start);
}

/* Shared logic for processing an include file */
static char *fd_realpath(int fd, const char *fallback)
{
    char path[PATH_MAX];

#ifdef F_GETPATH
    if (fcntl(fd, F_GETPATH, path) == 0) {
        char *canon = realpath(path, NULL);
        if (!canon)
            canon = vc_strdup(path);
        return canon;
    }
#endif

#ifdef __linux__
    char proc[64];
    snprintf(proc, sizeof(proc), "/proc/self/fd/%d", fd);
    ssize_t len = readlink(proc, path, sizeof(path) - 1);
    if (len >= 0) {
        if (len == (ssize_t)sizeof(path) - 1) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        path[len] = '\0';
        char *canon = realpath(path, NULL);
        if (!canon)
            canon = vc_strdup(path);
        return canon;
    }
#endif

    char *canon = realpath(fallback, NULL);
    if (!canon)
        canon = vc_strdup(fallback);
    return canon;
}

static int process_include_file(const char *fname, const char *chosen,
                                size_t idx, vector_t *macros, vector_t *conds,
                                strbuf_t *out, const vector_t *incdirs,
                                vector_t *stack, preproc_context_t *ctx)
{
    vector_t subconds;
    vector_init(&subconds, sizeof(cond_state_t));
    (void)fname; /* unused */
    int ok = 1;
    if (is_active(conds)) {
        if (!chosen) {
            ok = 0;
        } else {
            int fd = open(chosen, O_RDONLY);
            char *canon = NULL;
            if (fd >= 0) {
                canon = fd_realpath(fd, chosen);
                close(fd);
            }
            if (!canon)
                canon = vc_strdup(chosen);

            if (!pragma_once_contains(ctx, canon)) {
                if (include_stack_contains(stack, canon)) {
                    fprintf(stderr, "Include cycle detected: %s\n", canon);
                    ok = 0;
                } else if (!process_file(canon, macros, &subconds, out,
                                         incdirs, stack, ctx, idx)) {
                    if (errno)
                        perror(canon);
                    ok = 0;
                }
            }
            free(canon);
        }
    }
    vector_free(&subconds);
    return ok;
}


int handle_include(char *line, const char *dir, vector_t *macros,
                   vector_t *conds, strbuf_t *out,
                   const vector_t *incdirs, vector_t *stack,
                   preproc_context_t *ctx)
{
    strbuf_t expanded;
    strbuf_init(&expanded);
    if (!expand_line(line, macros, &expanded, 0, 0, ctx)) {
        strbuf_free(&expanded);
        return 0;
    }

    char endc;
    char *fname = parse_include_name(expanded.data ? expanded.data : line, &endc);
    if (!fname) {
        fprintf(stderr, "Malformed include directive\n");
        strbuf_free(&expanded);
        return 0;
    }
    int result = 1;
    size_t idx = SIZE_MAX;
    char *incpath = find_include_path(fname, endc, dir, incdirs, 0, &idx);
    int missing = (incpath == NULL);
    if (!process_include_file(fname, incpath, idx, macros, conds, out,
                              incdirs, stack, ctx))
        result = 0;
    if (missing)
        report_missing_include(line, fname, endc, dir, incdirs, 0);
    free(incpath);
    free(fname);
    strbuf_free(&expanded);
    return result;
}

int handle_include_next(char *line, const char *dir, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs, vector_t *stack,
                        preproc_context_t *ctx)
{
    (void)dir;
    strbuf_t expanded;
    strbuf_init(&expanded);
    if (!expand_line(line, macros, &expanded, 0, 0, ctx)) {
        strbuf_free(&expanded);
        return 0;
    }

    char endc;
    char *fname = parse_include_name(expanded.data ? expanded.data : line, &endc);
    if (!fname) {
        fprintf(stderr, "Malformed include directive\n");
        strbuf_free(&expanded);
        return 0;
    }
    if (stack->count <= 1) {
        fprintf(stderr, "#include_next is invalid outside a header\n");
        free(fname);
        strbuf_free(&expanded);
        return 0;
    }
    int result = 1;
    size_t cur = SIZE_MAX;
    if (stack->count) {
        const include_entry_t *e =
            &((include_entry_t *)stack->data)[stack->count - 1];
        cur = e->dir_index;
    }
    size_t idx = SIZE_MAX;
    size_t start_idx = (cur == (size_t)-1) ? 0 : cur + 1;
    char *incpath = find_include_path(fname, endc, NULL, incdirs,
                                      start_idx, &idx);
    int missing = (incpath == NULL);
    if (!process_include_file(fname, incpath, idx, macros, conds, out,
                              incdirs, stack, ctx))
        result = 0;
    if (missing)
        report_missing_include(line, fname, endc, NULL, incdirs, start_idx);
    free(incpath);
    free(fname);
    strbuf_free(&expanded);
    return result;
}

