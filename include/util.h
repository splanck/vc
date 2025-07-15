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
#include "cli.h"

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

/* Assemble a temporary file name using cli->obj_dir and open it.
 *
 * On success the path of the new file is stored in *out_path and the
 * descriptor is returned.  The caller is responsible for unlinking and
 * freeing the returned path.
 *
 * If an error occurs -1 is returned, *out_path is set to NULL and errno is
 * left to indicate the cause.  ENAMETOOLONG signals that the resulting path
 * would exceed PATH_MAX or snprintf detected truncation.  Other values come
 * from malloc, mkstemp or fcntl.
 */
int create_temp_file(const cli_options_t *cli, const char *prefix,
                     char **out_path);

/* Assemble an mkstemp template path using cli->obj_dir */
char *create_temp_template(const cli_options_t *cli, const char *prefix);

#endif /* VC_UTIL_H */
