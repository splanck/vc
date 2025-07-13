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
#include "strbuf.h"
#include <stdint.h>

/* Context used by the preprocessor.
 *
 * `pragma_once_files` stores headers that emitted `#pragma once` so
 * subsequent includes are ignored. `deps` records every file processed
 * including the initial source and all headers. The caller is
 * responsible for freeing both vectors via `preproc_context_free()`.
 */
/* default include depth limit */
#define DEFAULT_INCLUDE_DEPTH 20

typedef struct {
    vector_t pragma_once_files; /* vector of malloc'd char* paths */
    vector_t deps;              /* vector of malloc'd char* paths */
    vector_t pack_stack;        /* vector of size_t pack values */
    size_t pack_alignment;      /* current #pragma pack value */
    int in_comment;             /* tracks multi-line comment state */
    char *current_file;         /* file name for __FILE__ macro */
    long line_delta;            /* offset applied to __LINE__ */
    const char *file;           /* builtin __FILE__ value */
    size_t line;                /* builtin __LINE__ value */
    size_t column;              /* builtin column number */
    const char *func;           /* builtin __func__ value */
    const char *base_file;      /* builtin __BASE_FILE__ value */
    size_t include_level;       /* builtin __INCLUDE_LEVEL__ value */
    uint64_t counter;           /* builtin __COUNTER__ value */
    size_t max_include_depth;   /* maximum nested includes allowed */
    int system_header;          /* suppress warnings for current file */
} preproc_context_t;

/* Free the dependency lists stored in the context */
void preproc_context_free(preproc_context_t *ctx);

/* Preprocess the file at the given path.
 * The returned string must be freed by the caller.
 * Returns NULL on failure.
 */
char *preproc_run(preproc_context_t *ctx, const char *path,
                  const vector_t *include_dirs,
                  const vector_t *isystem_dirs,
                  const vector_t *defines,
                  const vector_t *undefines,
                  const char *sysroot);

/* Internal helpers shared across preprocessing modules */
int process_line(char *line, const char *dir, vector_t *macros,
                 vector_t *conds, strbuf_t *out,
                 const vector_t *incdirs, vector_t *stack,
                 preproc_context_t *ctx);
int process_file(const char *path, vector_t *macros, vector_t *conds,
                 strbuf_t *out, const vector_t *incdirs, vector_t *stack,
                 preproc_context_t *ctx, size_t idx);

#endif /* VC_PREPROC_FILE_H */
