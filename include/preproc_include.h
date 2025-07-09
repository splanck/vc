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

/* Conditional state used during directive processing */
typedef struct {
    int parent_active;
    int taking;
    int taken;
} cond_state_t;

/* Entry on the include stack */
typedef struct {
    char *path;
    size_t dir_index;
} include_entry_t;

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
