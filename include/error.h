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
void error_set(size_t line, size_t col);

/*
 * Print an error message to stderr using the position stored by
 * error_set().
 */
void error_print(const char *msg);

#endif /* VC_ERROR_H */
