/*
 * Error reporting helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_ERROR_H
#define VC_ERROR_H

#include <stddef.h>

/*
 * Save the given 1-based line and column so that the next call to
 * error_print() can report where the error occurred.  Passing 0 for
 * either value indicates an unknown position.
 */
/*
 * Store the given source position along with the active file and
 * current function name. The file and function parameters may be
 * NULL to leave the previous values unchanged.
 */
void error_set(size_t line, size_t col, const char *file, const char *func);

/*
 * Print an error message to stderr using the position stored by
 * error_set().
 */
void error_print(const char *msg);

/* Current context used by error diagnostics */
extern const char *error_current_file;
extern const char *error_current_function;

#endif /* VC_ERROR_H */
