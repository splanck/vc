/*
 * Utility function declarations.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_UTIL_H
#define VC_UTIL_H

#include <stddef.h>
#include "vector.h"

/* Duplicate a string using malloc. Returns NULL on allocation failure */
char *vc_strdup(const char *s);

/* Duplicate at most 'n' characters of a string. Returns NULL on allocation failure */
char *vc_strndup(const char *s, size_t n);

/* Print an out of memory message to stderr */
void vc_oom(void);

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

/* Release a vector of malloc'd strings */
void free_string_vector(vector_t *v);

/* Release a vector of macro_t elements */
void free_macro_vector(vector_t *v);

#endif /* VC_UTIL_H */
