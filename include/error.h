/*
 * Error reporting helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_ERROR_H
#define VC_ERROR_H

#include <stddef.h>

void error_set(size_t line, size_t col);
void error_print(const char *msg);

#endif /* VC_ERROR_H */
