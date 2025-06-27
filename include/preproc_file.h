/*
 * File processing entry points for the preprocessor.
 *
 * The preprocessor reads a source file, handles directives such as
 * `#include`, `#define`, `#ifdef` and friends and returns the expanded
 * text.  Included files are processed recursively using the caller
 * provided search paths.  Macro definitions are collected into a simple
 * vector and expanded on demand.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PREPROC_FILE_H
#define VC_PREPROC_FILE_H

#include "vector.h"

/* Preprocess the file at the given path.
 * The returned string must be freed by the caller.
 * Returns NULL on failure.
 */
char *preproc_run(const char *path, const vector_t *include_dirs);

#endif /* VC_PREPROC_FILE_H */
