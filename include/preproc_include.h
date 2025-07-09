/*
 * Include handling helpers for the preprocessor.
 *
 * Provides functions for processing #include and #include_next
 * directives used by the main preprocessor implementation.
 */

#ifndef VC_PREPROC_INCLUDE_H
#define VC_PREPROC_INCLUDE_H

#include "vector.h"
#include "strbuf.h"
#include "preproc_file.h"
#include "preproc_path.h"

/* Process a standard #include directive */
int handle_include(char *line, const char *dir, vector_t *macros,
                   vector_t *conds, strbuf_t *out,
                   const vector_t *incdirs, vector_t *stack,
                   preproc_context_t *ctx);

/* Process an #include_next directive */
int handle_include_next(char *line, const char *dir, vector_t *macros,
                        vector_t *conds, strbuf_t *out,
                        const vector_t *incdirs, vector_t *stack,
                        preproc_context_t *ctx);

#endif /* VC_PREPROC_INCLUDE_H */
