#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "preproc_includes.h"
#include "preproc_include.h"
#include "preproc_cond.h"
#include "preproc_file_io.h"
#include "preproc_path.h"
#include "preproc_builtin.h"
#include "preproc_macros.h"
#include "preproc_file.h"
#include "semantic_global.h"
#include "util.h"
#include "vector.h"
#include "strbuf.h"

#include <ctype.h>
#include "preproc_utils.h"



int handle_include_directive(char *line, const char *dir, vector_t *macros,
                             vector_t *conds, strbuf_t *out,
                             const vector_t *incdirs, vector_t *stack,
                             preproc_context_t *ctx)
{
    return handle_include(line, dir, macros, conds, out, incdirs, stack, ctx);
}

int handle_line_directive(char *line, const char *dir, vector_t *macros,
                          vector_t *conds, strbuf_t *out,
                          const vector_t *incdirs, vector_t *stack,
                          preproc_context_t *ctx)
{
    (void)dir; (void)incdirs; (void)stack;
    char *p = line;
    int gcc_style = 0;
    strbuf_t exp;
    strbuf_init(&exp);

    if (strncmp(line, "#line", 5) == 0 &&
        (line[5] == '\0' || isspace((unsigned char)line[5]))) {
        p = line + 5;
        p = skip_ws(p);
        if (!expand_line(p, macros, &exp, 0, 0, ctx)) {
            strbuf_free(&exp);
            return 0;
        }
        p = exp.data ? exp.data : "";
    } else if (line[0] == '#' && isdigit((unsigned char)line[1])) {
        gcc_style = 1;
        p = line + 1;
    } else {
        strbuf_free(&exp);
        return 1;
    }

    p = skip_ws(p);
    errno = 0;
    char *end;
    long long val = strtoll(p, &end, 10);
    if (p == end || errno != 0 || val > INT_MAX || val <= 0) {
        fprintf(stderr, "Invalid line number in #line directive\n");
        strbuf_free(&exp);
        return 0;
    }
    p = end;
    int lineno = (int)val;
    p = skip_ws(p);
    char *fname = NULL;
    if (*p == '"') {
        p++;
        char *fstart = p;
        while (*p && *p != '"')
            p++;
        if (*p == '"') {
            fname = vc_strndup(fstart, (size_t)(p - fstart));
            p++;
        }
    }
    p = skip_ws(p);
    char *flags = NULL;
    if (gcc_style && *p)
        flags = vc_strdup(p);

    if (is_active(conds)) {
        if (strbuf_appendf(out, "# %d", lineno) < 0) {
            free(fname);
            free(flags);
            strbuf_free(&exp);
            return 0;
        }
        if (fname && strbuf_appendf(out, " \"%s\"", fname) < 0) {
            free(fname);
            free(flags);
            strbuf_free(&exp);
            return 0;
        }
        if (flags && strbuf_appendf(out, " %s", flags) < 0) {
            free(fname);
            free(flags);
            strbuf_free(&exp);
            return 0;
        }
        if (strbuf_append(out, "\n") < 0) {
            free(fname);
            free(flags);
            strbuf_free(&exp);
            return 0;
        }
        preproc_apply_line_directive(ctx, fname ? fname : NULL, lineno);
    }
    free(fname);
    free(flags);
    strbuf_free(&exp);
    return 1;
}


