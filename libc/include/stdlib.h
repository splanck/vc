/*
 * Minimal stdlib declarations for vc's internal libc.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_STDLIB_H
#define VC_STDLIB_H

#include <stddef.h>

void exit(int);
void *malloc(size_t);
void free(void *);

#endif /* VC_STDLIB_H */
