/*
 * Minimal source preprocessor.
 *
 * Supports '#include "file"', object-like '#define NAME value' and simple
 * single-parameter macros of the form '#define NAME(arg) expr'. Expansion is
 * purely textual and does not recognize strings or comments. Conditional
 * directives remain unimplemented.
 */

#ifndef VC_PREPROC_H
#define VC_PREPROC_H

/* Preprocess the file at the given path.
 * The returned string must be freed by the caller.
 * Returns NULL on failure.
 */
char *preproc_run(const char *path);

#endif /* VC_PREPROC_H */
