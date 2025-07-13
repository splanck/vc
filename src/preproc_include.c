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
#include "preproc_cond.h"
#include "preproc_file_io.h"
#include "preproc_path.h"
#include "preproc_file.h"
#include "preproc_macros.h"

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

