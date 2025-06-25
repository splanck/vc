/*
 * Minimal source preprocessor.
 *
 * Supports only '#include "file"' and simple object-like '#define NAME value'.
 * Expansion is naive: macro names are replaced as plain identifiers without
 * parameter handling. Conditionals and other directives are not implemented.
 */

#ifndef VC_PREPROC_H
#define VC_PREPROC_H

/* Preprocess the file at the given path.
 * The returned string must be freed by the caller.
 * Returns NULL on failure.
 */
char *preproc_run(const char *path);

#endif /* VC_PREPROC_H */
