/*
 * Macro handling for the preprocessor.
 *
 * Defines the `macro_t` structure and helper routines used to store,
 * expand and query macros.  Expansion occurs line by line and is
 * recursive so macro bodies may themselves contain macro invocations.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PREPROC_MACROS_H
#define VC_PREPROC_MACROS_H

#include "vector.h"
#include "strbuf.h"

/*
 * Stored macro definition.
 *
 * The strings pointed to by "name" and "value" as well as each entry in
 * the "params" vector are allocated on the heap.  Ownership of these
 * allocations belongs to the macro instance and they are released by
 * macro_free().  add_macro() creates a fully self-contained macro_t by
 * duplicating the provided name and value strings and taking ownership of
 * the parameter names supplied in the vector.
 */
typedef struct {
    char *name;       /* malloc'd name string */
    vector_t params;  /* vector of malloc'd char* parameter names */
    int variadic;     /* non-zero when macro accepts variable arguments */
    char *value;      /* malloc'd macro body */
} macro_t;

/* Free memory used by a macro */
void macro_free(macro_t *m);

/* Expand macros in one line */
int expand_line(const char *line, vector_t *macros, strbuf_t *out,
                size_t column, int depth);

/* Check whether a macro exists */
int is_macro_defined(vector_t *macros, const char *name);

/* Remove all definitions of a macro */
void remove_macro(vector_t *macros, const char *name);

/* Update builtin macro expansion context */
void preproc_set_location(const char *file, size_t line, size_t column);

/* Set the current function name for __func__ expansion */
void preproc_set_function(const char *name);

#endif /* VC_PREPROC_MACROS_H */
