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

/* Context used by the preprocessor.
 *
 * `pragma_once_files` stores headers that emitted `#pragma once` so
 * subsequent includes are ignored. `deps` records every file processed
 * including the initial source and all headers. The caller is
 * responsible for freeing both vectors via `preproc_context_free()`.
 */
typedef struct {
    vector_t pragma_once_files; /* vector of malloc'd char* paths */
    vector_t deps;              /* vector of malloc'd char* paths */
    vector_t pack_stack;        /* vector of size_t pack values */
    size_t pack_alignment;      /* current #pragma pack value */
} preproc_context_t;

/* Free the dependency lists stored in the context */
void preproc_context_free(preproc_context_t *ctx);

/* Preprocess the file at the given path.
 * The returned string must be freed by the caller.
 * Returns NULL on failure.
 */
char *preproc_run(preproc_context_t *ctx, const char *path,
                  const vector_t *include_dirs, const vector_t *defines,
                  const vector_t *undefines);

#endif /* VC_PREPROC_FILE_H */
