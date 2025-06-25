/*
 * Minimal source preprocessor.
 *
 * Supports only '#include "file"' and simple object-like '#define NAME value'.
 * Expansion is naive: macro names are replaced as plain identifiers without
 * parameter handling. Conditionals and other directives are not implemented.
 */

#ifndef VC_PREPROC_H
#define VC_PREPROC_H

/* Preprocess the file at the given path using optional include search paths.
 * 'search_paths' is an array of directory strings to search when processing
 * '#include "file"' directives.  The returned string must be freed by the
 * caller. Returns NULL on failure.
 */
char *preproc_run(const char *path,
                  const char **search_paths,
                  size_t num_paths);

#endif /* VC_PREPROC_H */
