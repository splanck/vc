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

/* Active file and function for diagnostics */
const char *error_current_file = "";
const char *error_current_function = NULL;
bool error_use_color = true;

/* Stored source location for the next error message. */
static const char *error_file = "";
static const char *error_func = NULL;
static size_t error_line = 0;
static size_t error_column = 0;

/*
 * Remember the given source position for use by error_print().
 * The line and column numbers are 1-based; passing 0 for either
 * value records an unknown location.
 */
void error_set(size_t line, size_t col, const char *file, const char *func)
{
    error_line = line;
    error_column = col;
    error_file = file ? file : error_current_file;
    error_func = func ? func : error_current_function;
}

/*
 * Output "msg" followed by the location previously stored with
 * error_set().  The message is printed to stderr in a
 * "line/column" format and is intended for compiler diagnostics.
 */
void error_print(const char *msg)
{
    /* Use colored output only when error_use_color is true and
     * isatty(stderr) reports a terminal. */
    int color = error_use_color && isatty(fileno(stderr));
    /* \x1b[1;31m sets bold red text; \x1b[0m resets formatting. */
    if (color)
        fprintf(stderr, "\x1b[1;31m");
    if (error_func)
        fprintf(stderr, "%s:%zu:%zu: %s: %s", error_file, error_line,
                error_column, error_func, msg);
    else
        fprintf(stderr, "%s:%zu:%zu: %s", error_file, error_line,
                error_column, msg);
    if (color)
        fprintf(stderr, "\x1b[0m");
    fputc('\n', stderr);
}

