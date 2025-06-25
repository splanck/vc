/*
 * Utility function declarations.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_UTIL_H
#define VC_UTIL_H

/* Duplicate a string using malloc */
char *vc_strdup(const char *s);

/* Allocate memory or exit on failure */
void *vc_alloc_or_exit(size_t size);

/* Reallocate memory or exit on failure */
void *vc_realloc_or_exit(void *ptr, size_t size);

/* Read entire file into a NUL-terminated buffer */
char *vc_read_file(const char *path);

#endif /* VC_UTIL_H */
