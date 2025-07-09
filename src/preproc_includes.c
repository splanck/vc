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
#include "semantic_global.h"
#include "util.h"
#include "vector.h"
#include "strbuf.h"

/* Advance P past spaces and tabs and return the updated pointer */
static char *skip_ws(char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

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

/* Append a #pragma directive to the output when active */
static int handle_pragma(char *line, vector_t *conds, strbuf_t *out)
{
    if (is_active(conds)) {
        if (strbuf_append(out, line) != 0)
            return 0;
        if (strbuf_append(out, "\n") != 0)
            return 0;
    }
    return 1;
}

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
    (void)dir; (void)macros; (void)incdirs; (void)stack; (void)ctx;
    char *p = line + 5;
    p = skip_ws(p);
    errno = 0;
    char *end;
    long long val = strtoll(p, &end, 10);
    if (p == end || errno != 0 || val > INT_MAX || val <= 0) {
        fprintf(stderr, "Invalid line number in #line directive\n");
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
        if (*p == '"')
            fname = vc_strndup(fstart, (size_t)(p - fstart));
    }
    if (is_active(conds)) {
        if (strbuf_appendf(out, "# %d", lineno) < 0) {
            free(fname);
            return 0;
        }
        if (fname && strbuf_appendf(out, " \"%s\"", fname) < 0) {
            free(fname);
            return 0;
        }
        if (strbuf_append(out, "\n") < 0) {
            free(fname);
            return 0;
        }
    }
    free(fname);
    return 1;
}

int handle_pragma_directive(char *line, const char *dir, vector_t *macros,
                            vector_t *conds, strbuf_t *out,
                            const vector_t *incdirs, vector_t *stack,
                            preproc_context_t *ctx)
{
    (void)dir; (void)macros; (void)incdirs;
    char *p = line + 7; /* skip '#pragma' */
    p = skip_ws(p);
    if (strncmp(p, "once", 4) == 0) {
        p += 4;
        p = skip_ws(p);
        if (*p == '\0' && stack->count) {
            const include_entry_t *e =
                &((include_entry_t *)stack->data)[stack->count - 1];
            const char *cur = e->path;
            if (!pragma_once_add(ctx, cur))
                return 0;
        }
    } else if (strncmp(p, "pack", 4) == 0) {
        p += 4;
        p = skip_ws(p);
        if (strncmp(p, "(push", 5) == 0) {
            p += 5; /* after '(push' */
            if (*p == ',')
                p++;
            p = skip_ws(p);
            errno = 0;
            char *end;
            long val = strtol(p, &end, 10);
            if (errno == 0 && end != p) {
                p = end;
                p = skip_ws(p);
                if (*p == ')') {
                    vector_push(&ctx->pack_stack, &ctx->pack_alignment);
                    ctx->pack_alignment = (size_t)val;
                    semantic_set_pack(ctx->pack_alignment);
                }
            }
        } else if (strncmp(p, "(pop)", 5) == 0) {
            if (ctx->pack_stack.count) {
                ctx->pack_alignment =
                    ((size_t *)ctx->pack_stack.data)[ctx->pack_stack.count - 1];
                ctx->pack_stack.count--;
            } else {
                ctx->pack_alignment = 0;
            }
            semantic_set_pack(ctx->pack_alignment);
        }
        (void)stack;
        return 1; /* do not emit pragma line */
    }
    (void)stack;
    return handle_pragma(line, conds, out);
}

