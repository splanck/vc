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

/* Context used by the preprocessor.  Currently only tracks files
 * processed after encountering '#pragma once'.
 */
typedef struct {
    vector_t pragma_once_files; /* vector of malloc'd char* paths */
} preproc_context_t;

/* Preprocess the file at the given path.
 * The returned string must be freed by the caller.
 * Returns NULL on failure.
 */
char *preproc_run(preproc_context_t *ctx, const char *path,
                  const vector_t *include_dirs, const vector_t *defines,
                  const vector_t *undefines);

#endif /* VC_PREPROC_FILE_H */
