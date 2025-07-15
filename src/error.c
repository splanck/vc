#define _POSIX_C_SOURCE 200809L
/*
 * Simple error reporting utilities.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include "error.h"

/* Active diagnostic context */
error_context_t error_ctx = {"", NULL, 0, 0};
bool error_use_color = true;

/*
 * Remember the given source position for use by error_print().
 * The line and column numbers are 1-based; passing 0 for either
 * value records an unknown location.
 */
void error_set(error_context_t *ctx, size_t line, size_t col,
               const char *file, const char *func)
{
    ctx->line = line;
    ctx->column = col;
    if (file)
        ctx->file = file;
    if (func)
        ctx->function = func;
}

/*
 * Output "msg" followed by the location previously stored with
 * error_set().  The message is printed to stderr in a
 * "line/column" format and is intended for compiler diagnostics.
 */
void error_print(const error_context_t *ctx, const char *msg)
{
    int color = error_use_color && isatty(fileno(stderr));
    if (color)
        fprintf(stderr, "\x1b[1;31m");
    if (ctx->function)
        fprintf(stderr, "%s:%zu:%zu: %s: %s", ctx->file, ctx->line,
                ctx->column, ctx->function, msg);
    else
        fprintf(stderr, "%s:%zu:%zu: %s", ctx->file, ctx->line,
                ctx->column, msg);
    if (color)
        fprintf(stderr, "\x1b[0m");
    fputc('\n', stderr);
}

