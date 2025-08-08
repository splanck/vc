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

/*
 * Duplicate a string using malloc.
 *
 * Returns NULL on allocation failure or if 's' is NULL.
 */
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

/*
 * Convert string to size_t, returning 1 on success.
 *
 * Negative values are rejected.
 */
int vc_strtoul_size(const char *s, size_t *out);

/*
 * Convert string to unsigned, returning 1 on success.
 *
 * Negative values are rejected.
 */
int vc_strtoul_unsigned(const char *s, unsigned *out);

/* Release a vector of malloc'd strings */
void free_string_vector(vector_t *v);

/* Release a vector of macro_t elements */
void free_macro_vector(vector_t *v);

/* Release a vector of func_t* elements */
void free_func_list_vector(vector_t *v);

/* Release a vector of stmt_t* elements */
void free_glob_list_vector(vector_t *v);

/*
 * Assemble an mkstemp template path using cli->obj_dir or TMPDIR.
 * Returns a newly allocated string on success or NULL on failure.
 *
 * Possible errno values:
 *   ENAMETOOLONG - resulting path would exceed PATH_MAX or snprintf truncated
 *   others       - from malloc or snprintf
 */
char *create_temp_template(const cli_options_t *cli, const char *prefix);

/*
 * Create and open the temporary file described by tmpl.  The file is opened
 * with O_CLOEXEC using mkostemp when available; otherwise mkstemp is used and
 * FD_CLOEXEC is set via fcntl.
 */
int open_temp_file(char *tmpl);

#endif /* VC_UTIL_H */
