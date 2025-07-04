/*
 * Utility function declarations.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_UTIL_H
#define VC_UTIL_H

#include "ast.h"

/* Duplicate a string using malloc. Returns NULL on allocation failure */
char *vc_strdup(const char *s);

/* Duplicate at most 'n' characters of a string. Returns NULL on allocation failure */
char *vc_strndup(const char *s, size_t n);

/* Allocate memory or exit on failure */
void *vc_alloc_or_exit(size_t size);

/* Reallocate memory or exit on failure */
void *vc_realloc_or_exit(void *ptr, size_t size);

/* Read entire file into a NUL-terminated buffer */
char *vc_read_file(const char *path);

/* Convert string to size_t, returning 1 on success */
int vc_strtoul_size(const char *s, size_t *out);

/* Convert string to unsigned, returning 1 on success */
int vc_strtoul_unsigned(const char *s, unsigned *out);

/* Allocate and duplicate an array of members */
int copy_members(union_member_t **dst, const union_member_t *src, size_t count);

#endif /* VC_UTIL_H */
