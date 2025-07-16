#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "preproc_file.h"
#include "preproc_macros.h"
#include "preproc_cond.h"
#include "preproc_include.h"
#include "preproc_includes.h"
#include "preproc_builtin.h"
#include "preproc_path.h"
#include "preproc_file_io.h"
#include "semantic_global.h"
#include "util.h"
#include "vector.h"
#include "strbuf.h"
#include "preproc_utils.h"

/* Remove comments from S, tracking multi-line state in *IN_COMMENT.
 * Comment markers inside string or character literals are ignored. */
static void strip_comments(char *s, int *in_comment)
{
    char *out = s;
    size_t i = 0;
    int in_quote = 0;
    int escape = 0;
    char quote = '\0';

    while (s[i]) {
        if (*in_comment) {
            if (s[i] == '*' && s[i + 1] == '/') {
                i += 2;
                *in_comment = 0;
                continue;
            }
            i++;
            continue;
        }
        char c = s[i];
        if (!in_quote && c == '/' && s[i + 1] == '/')
            break;
        if (!in_quote && c == '/' && s[i + 1] == '*') {
            *in_comment = 1;
            i += 2;
            continue;
        }
        out[0] = c;
        out++;
        if (in_quote) {
            if (escape) {
                escape = 0;
            } else if (c == '\\') {
                escape = 1;
            } else if (c == quote) {
                in_quote = 0;
            }
        } else if (c == '"' || c == '\'') {
            in_quote = 1;
            quote = c;
        }
        i++;
    }
    *out = '\0';
}






/* forward declaration for recursive include handling */
int process_file(const char *path, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs, vector_t *stack,
                        preproc_context_t *ctx, size_t idx);

/* Small helpers used by process_file */

static int handle_directive(char *line, const char *dir, vector_t *macros,
                            vector_t *conds, strbuf_t *out,
                            const vector_t *incdirs, vector_t *stack,
                            preproc_context_t *ctx);

/* Process one line of input.  Leading whitespace is skipped before
 * dispatching to the directive handlers. */
int process_line(char *line, const char *dir, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs, vector_t *stack,
                        preproc_context_t *ctx)
{
    strip_comments(line, &ctx->in_comment);
    if (ctx->in_comment && *line == '\0')
        return 1;
    line = skip_ws(line);
    if (*line == '#') {
        char *p = line + 1;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            return 1;
        if (p != line + 1)
            memmove(line + 1, p, strlen(p) + 1);
    }
    return handle_directive(line, dir, macros, conds, out, incdirs, stack, ctx);
}

/* Remove a macro defined earlier when #undef is seen. */
static int handle_undef_directive(char *line, const char *dir, vector_t *macros,
                                  vector_t *conds, strbuf_t *out,
                                  const vector_t *incdirs,
                                  vector_t *stack,
                                  preproc_context_t *ctx)
{
    (void)dir; (void)out; (void)incdirs; (void)stack; (void)ctx;
    char *n = line + 6;
    n = skip_ws(n);
    char *id = n;
    while (isalnum((unsigned char)*n) || *n == '_')
        n++;
    *n = '\0';
    if (is_active(conds))
        remove_macro(macros, id);
    return 1;
}

/* Emit an error message and abort preprocessing when active. */
static int handle_error_directive(char *line, const char *dir,
                                  vector_t *macros, vector_t *conds,
                                  strbuf_t *out,
                                  const vector_t *incdirs,
                                  vector_t *stack,
                                  preproc_context_t *ctx)
{
    (void)dir; (void)incdirs; (void)stack; (void)out;
    char *msg = line + 6; /* skip '#error' */
    msg = skip_ws(msg);
    if (is_active(conds)) {
        strbuf_t tmp;
        strbuf_init(&tmp);
        if (*msg) {
            if (!expand_line(msg, macros, &tmp, 0, 0, ctx)) {
                strbuf_free(&tmp);
                return 0;
            }
        } else {
            if (strbuf_append(&tmp, "preprocessor error") != 0) {
                strbuf_free(&tmp);
                return 0;
            }
        }
        const char *file = ctx->current_file ? ctx->current_file : "";
        fprintf(stderr, "%s:%zu: %s\n", file, preproc_get_line(ctx), tmp.data);
        strbuf_free(&tmp);
        return 0;
    }
    return 1;
}

/* Emit a warning message but continue preprocessing when active. */
static int handle_warning_directive(char *line, const char *dir,
                                    vector_t *macros, vector_t *conds,
                                    strbuf_t *out,
                                    const vector_t *incdirs,
                                    vector_t *stack,
                                    preproc_context_t *ctx)
{
    (void)dir; (void)macros; (void)out; (void)incdirs; (void)stack;
    char *msg = line + 8; /* skip '#warning' */
    msg = skip_ws(msg);
    if (is_active(conds)) {
        const char *file = ctx->current_file ? ctx->current_file : "";
        fprintf(stderr, "%s:%zu: %s\n", file, preproc_get_line(ctx),
                *msg ? msg : "preprocessor warning");
    }
    return 1;
}

int handle_pragma_directive(char *line, const char *dir, vector_t *macros,
                            vector_t *conds, strbuf_t *out,
                            const vector_t *incdirs, vector_t *stack,
                            preproc_context_t *ctx)
{
    (void)dir; (void)incdirs;
    char *arg = line + 7; /* skip '#pragma' */
    arg = skip_ws(arg);
    strbuf_t exp;
    strbuf_init(&exp);
    if (!expand_line(arg, macros, &exp, 0, 0, ctx)) {
        strbuf_free(&exp);
        return 0;
    }
    if (strcmp(arg, exp.data ? exp.data : "") != 0) {
        strbuf_t tmp;
        strbuf_init(&tmp);
        strbuf_appendf(&tmp, "#pragma %s", exp.data ? exp.data : "");
        char *dup = vc_strdup(tmp.data ? tmp.data : "");
        strbuf_free(&tmp);
        if (!dup) {
            strbuf_free(&exp);
            vc_oom();
            return 0;
        }
        strbuf_free(&exp);
        int r = process_line(dup, dir, macros, conds, out, incdirs, stack, ctx);
        free(dup);
        return r;
    }
    char *p = exp.data;
    p = skip_ws(p);
    if (strncmp(p, "once", 4) == 0) {
        p += 4;
        p = skip_ws(p);
        if (*p == '\0' && stack->count) {
            const include_entry_t *e =
                &((include_entry_t *)stack->data)[stack->count - 1];
            const char *cur = e->path;
            if (!pragma_once_add(ctx, cur)) {
                strbuf_free(&exp);
                return 0;
            }
        }
        strbuf_free(&exp);
        (void)conds;
        return 1;
    } else if (strncmp(p, "pack", 4) == 0) {
        p += 4;
        p = skip_ws(p);
        if (strncmp(p, "(push", 5) == 0) {
            p += 5;
            p = skip_ws(p);
            if (*p == ')') {
                vector_push(&ctx->pack_stack, &ctx->pack_alignment);
            } else {
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
        strbuf_free(&exp);
        (void)stack;
        return 1;
    } else if (strncmp(p, "system_header", 13) == 0) {
        ctx->system_header = 1;
        strbuf_free(&exp);
        (void)conds; (void)stack;
        return 1;
    } else if (strncmp(p, "GCC", 3) == 0) {
        p += 3;
        p = skip_ws(p);
        if (strncmp(p, "system_header", 13) == 0) {
            ctx->system_header = 1;
            strbuf_free(&exp);
            (void)conds; (void)stack;
            return 1;
        }
    }
    if (is_active(conds)) {
        if (strbuf_appendf(out, "#pragma %s\n", exp.data ? exp.data : "") < 0) {
            strbuf_free(&exp);
            return 0;
        }
    }
    strbuf_free(&exp);
    (void)stack;
    return 1;
}


/* Update conditional state based on #if/#else/#endif directives. */
static int handle_conditional_directive(char *line, const char *dir,
                                        vector_t *macros, vector_t *conds,
                                        strbuf_t *out,
                                        const vector_t *incdirs,
                                        vector_t *stack,
                                        preproc_context_t *ctx)
{
    (void)out; (void)ctx;
    return handle_conditional(line, dir, macros, conds, incdirs, stack, ctx);
}

/*
 * Expand a regular text line and append it to the output when the current
 * conditional stack is active.
 */
static int handle_text_line(char *line, const char *dir, vector_t *macros,
                            vector_t *conds, strbuf_t *out,
                            const vector_t *incdirs, vector_t *stack,
                            preproc_context_t *ctx)
{
    (void)dir; (void)incdirs; (void)stack; (void)ctx;
    int ok = 1;
    if (is_active(conds)) {
        strbuf_t tmp;
        strbuf_init(&tmp);
        if (!expand_line(line, macros, &tmp, 0, 0, ctx)) {
            ok = 0;
        } else if (strbuf_append(&tmp, "\n") != 0) {
            ok = 0;
        }
        if (ok && strbuf_append(out, tmp.data) != 0)
            ok = 0;
        strbuf_free(&tmp);
    }
    return ok;
}

/*
 * Handle a preprocessor directive or regular text line.  Dispatches to the
 * specific handler for the directive and falls back to text expansion when the
 * line is not a recognised directive.
 */
typedef int (*directive_fn_t)(char *, const char *, vector_t *, vector_t *,
                              strbuf_t *, const vector_t *, vector_t *,
                              preproc_context_t *);

enum { SPACE_NONE, SPACE_BLANK, SPACE_ANY };

typedef struct {
    const char    *name;
    int            space;
    directive_fn_t handler;
} directive_entry_t;

typedef struct {
    size_t start;
    size_t count;
} directive_bucket_t;

static const directive_entry_t directive_table[] = {
    {"#define",  SPACE_BLANK, handle_define_directive},
    {"#elif",    SPACE_ANY,   handle_conditional_directive},
    {"#else",    SPACE_NONE,  handle_conditional_directive},
    {"#endif",   SPACE_NONE,  handle_conditional_directive},
    {"#error",   SPACE_ANY,   handle_error_directive},
    {"#ifdef",   SPACE_ANY,   handle_conditional_directive},
    {"#ifndef",  SPACE_ANY,   handle_conditional_directive},
    {"#if",      SPACE_ANY,   handle_conditional_directive},
    {"#include",      SPACE_BLANK, handle_include_directive},
    {"#include_next", SPACE_BLANK, handle_include_next},
    {"#line",         SPACE_ANY,   handle_line_directive},
    {"#pragma",  SPACE_ANY,   handle_pragma_directive},
    {"#undef",   SPACE_ANY,   handle_undef_directive},
    {"#warning", SPACE_ANY,   handle_warning_directive},
};

static const directive_bucket_t directive_buckets[26] = {
    ['d' - 'a'] = {0, 1},  /* #define */
    ['e' - 'a'] = {1, 4},  /* #elif, #else, #endif, #error */
    ['i' - 'a'] = {5, 5},  /* #ifdef, #ifndef, #if, #include, #include_next */
    ['l' - 'a'] = {10, 1}, /* #line */
    ['p' - 'a'] = {11, 1}, /* #pragma */
    ['u' - 'a'] = {12, 1}, /* #undef */
    ['w' - 'a'] = {13, 1}, /* #warning */
};

static const directive_entry_t *lookup_directive(const char *line)
{
    if (line[0] != '#')
        return NULL;

    char c = line[1];
    if (c < 'a' || c > 'z')
        return NULL;

    directive_bucket_t bucket = directive_buckets[c - 'a'];
    for (size_t i = 0; i < bucket.count; i++) {
        const directive_entry_t *d = &directive_table[bucket.start + i];
        size_t len = strlen(d->name);
        if (strncmp(line, d->name, len) == 0) {
            char next = line[len];
            if (d->space == SPACE_BLANK) {
                if (next == ' ' || next == '\t')
                    return d;
            } else if (d->space == SPACE_ANY) {
                if (isspace((unsigned char)next))
                    return d;
            } else {
                return d;
            }
        }
    }

    return NULL;
}

static int handle_directive(char *line, const char *dir, vector_t *macros,
                            vector_t *conds, strbuf_t *out,
                            const vector_t *incdirs, vector_t *stack,
                            preproc_context_t *ctx)
{
    const directive_entry_t *d = lookup_directive(line);
    if (d)
        return d->handler(line, dir, macros, conds, out, incdirs, stack, ctx);

    if (line[0] == '#' && isdigit((unsigned char)line[1]))
        return handle_line_directive(line, dir, macros, conds, out,
                                     incdirs, stack, ctx);

    return handle_text_line(line, dir, macros, conds, out, incdirs, stack, ctx);
}
