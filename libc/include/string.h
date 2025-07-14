/*
 * Minimal string declarations for vc's internal libc.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_STRING_H
#define VC_STRING_H

#include <stddef.h>

size_t strlen(const char *);
void *memcpy(void *, const void *, size_t);

#endif /* VC_STRING_H */
