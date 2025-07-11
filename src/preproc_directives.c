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
#include "util.h"
#include "vector.h"
#include "strbuf.h"

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

/* Advance P past spaces and tabs and return the updated pointer */
static char *skip_ws(char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}



/* Record PATH in the dependency list if not already present */

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


/* Canonicalize PATH and push it on the include stack */

/* forward declaration for recursive include handling */
int process_file(const char *path, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs, vector_t *stack,
                        preproc_context_t *ctx, size_t idx);

/*
 * Locate the full path of an include file.
 *
 * The search order mirrors traditional compiler behaviour:
 *   1. When the include uses quotes and a directory for the current file
 *      is provided, that directory is checked first.
 *   2. Each path in "incdirs" is consulted in order.
 *   3. For quoted includes the plain filename is tried relative to the
 *      working directory.
 *   4. Finally the built in standard include directories are searched.
 *
 * The resulting path is written to "out_path" if found and a pointer to
 * it is returned.  NULL is returned when the file does not exist in any
 * of the locations.
*/


/* Small helpers used by process_file */

/*
 * Read a file and split it into NUL terminated lines.  The returned
 * text buffer holds the line data and must remain valid for the lifetime
 * of the line array.  The caller must free both using the cleanup helper.
 * NULL is returned on I/O errors.
 */
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
        if (p != line + 1)
            memmove(line + 1, p, strlen(p) + 1);
    }
    return handle_directive(line, dir, macros, conds, out, incdirs, stack, ctx);
}

/* Free resources allocated by process_file */
/* Free a vector of parameter names */
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
    (void)dir; (void)macros; (void)out; (void)incdirs; (void)stack; (void)ctx;
    char *msg = line + 6; /* skip '#error' */
    msg = skip_ws(msg);
    if (is_active(conds)) {
        fprintf(stderr, "%s\n", *msg ? msg : "preprocessor error");
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
    (void)dir; (void)macros; (void)out; (void)incdirs; (void)stack; (void)ctx;
    char *msg = line + 8; /* skip '#warning' */
    msg = skip_ws(msg);
    if (is_active(conds))
        fprintf(stderr, "%s\n", *msg ? msg : "preprocessor warning");
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
    (void)dir; (void)out; (void)incdirs; (void)stack; (void)ctx;
    return handle_conditional(line, macros, conds);
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
        if (!expand_line(line, macros, &tmp, 0, 0))
            ok = 0;
        else
            strbuf_append(&tmp, "\n");
        if (ok)
            strbuf_append(out, tmp.data);
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

    return handle_text_line(line, dir, macros, conds, out, incdirs, stack, ctx);
}
