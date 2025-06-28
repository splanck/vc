/*
 * Simple error reporting utilities.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include "error.h"

/* Active file and function for diagnostics */
const char *error_current_file = "";
const char *error_current_function = NULL;

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
    if (error_func)
        fprintf(stderr, "%s:%zu:%zu: %s: %s\n", error_file, error_line, error_column,
                error_func, msg);
    else
        fprintf(stderr, "%s:%zu:%zu: %s\n", error_file, error_line, error_column,
                msg);
}

