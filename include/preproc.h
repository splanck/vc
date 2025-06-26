/*
 * Minimal source preprocessor.
 *
 * Supports '#include "file"' and '#include <file>', object-like '#define NAME value' and simple
 * single-parameter macros of the form '#define NAME(arg) expr'. Expansion is
 * purely textual and does not recognize strings or comments.  Basic
 * conditional directives ('#if', '#ifdef', '#ifndef', '#elif', '#else',
 * '#endif') are supported with simple expression evaluation.
 */

#ifndef VC_PREPROC_H
#define VC_PREPROC_H

#include "vector.h"

/* Preprocess the file at the given path.
 * The returned string must be freed by the caller.
 * Returns NULL on failure.
 */
char *preproc_run(const char *path, const vector_t *include_dirs);

#endif /* VC_PREPROC_H */
