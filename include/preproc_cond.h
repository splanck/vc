/*
 * Conditional directive helpers for the preprocessor.
 *
 * Provides functions to manage the conditional state stack and
 * dispatch #if/#else/#endif directives during preprocessing.
 */

#ifndef VC_PREPROC_COND_H
#define VC_PREPROC_COND_H

#include "vector.h"

/* Conditional state used during directive processing */
typedef struct {
    int parent_active;
    int taking;
    int taken;
} cond_state_t;

/* Push a new state for #ifdef/#ifndef directives */
int cond_push_ifdef_common(char *line, vector_t *macros,
                           vector_t *conds, int neg);
int cond_push_ifdef(char *line, vector_t *macros, vector_t *conds);
int cond_push_ifndef(char *line, vector_t *macros, vector_t *conds);

/* Push a new state for a generic #if expression */
int cond_push_ifexpr(char *line, vector_t *macros, vector_t *conds);

/* Handle conditional branches */
void cond_handle_elif(char *line, vector_t *macros, vector_t *conds);
void cond_handle_else(vector_t *conds);
void cond_handle_endif(vector_t *conds);

/* Dispatch a conditional directive */
int handle_conditional(char *line, vector_t *macros, vector_t *conds);

#endif /* VC_PREPROC_COND_H */
