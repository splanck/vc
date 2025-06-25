/*
 * Simple error reporting utilities.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include "error.h"

/* Stored source location for the next error message */
static size_t error_line = 0;
static size_t error_column = 0;

/*
 * Remember the given source position for use by error_print().  The
 * values are 1-based; a line or column of 0 indicates an unknown
 * position.
 */
void error_set(size_t line, size_t col)
{
    error_line = line;
    error_column = col;
}

/*
 * Output "msg" followed by the location previously stored with
 * error_set().  The message is printed to stderr in a
 * "line/column" format.
 */
void error_print(const char *msg)
{
    fprintf(stderr, "%s at line %zu, column %zu\n", msg, error_line, error_column);
}

