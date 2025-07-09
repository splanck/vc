/*
 * Directive wrappers for include and related directives.
 */

#ifndef VC_PREPROC_INCLUDES_H
#define VC_PREPROC_INCLUDES_H

#include "vector.h"
#include "strbuf.h"
#include "preproc_file.h"

int handle_include_directive(char *line, const char *dir, vector_t *macros,
                             vector_t *conds, strbuf_t *out,
                             const vector_t *incdirs, vector_t *stack,
                             preproc_context_t *ctx);

int handle_line_directive(char *line, const char *dir, vector_t *macros,
                          vector_t *conds, strbuf_t *out,
                          const vector_t *incdirs, vector_t *stack,
                          preproc_context_t *ctx);

int handle_pragma_directive(char *line, const char *dir, vector_t *macros,
                            vector_t *conds, strbuf_t *out,
                            const vector_t *incdirs, vector_t *stack,
                            preproc_context_t *ctx);

#endif /* VC_PREPROC_INCLUDES_H */
