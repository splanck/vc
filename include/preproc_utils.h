/*
 * Common helper functions for the preprocessor.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PREPROC_UTILS_H
#define VC_PREPROC_UTILS_H

#include <ctype.h>
#include "preproc_cond.h"

/* Advance P past whitespace and return the updated pointer */
static inline char *skip_ws(char *p)
{
    while (isspace((unsigned char)*p))
        p++;
    return p;
}

/* Return 1 if all conditional states on the stack are active */
static inline int stack_active(vector_t *conds)
{
    for (size_t i = 0; i < conds->count; i++) {
        cond_state_t *c = &((cond_state_t *)conds->data)[i];
        if (!c->taking)
            return 0;
    }
    return 1;
}

/* Wrapper used by directive handlers */
static inline int is_active(vector_t *conds)
{
    return stack_active(conds);
}

#endif /* VC_PREPROC_UTILS_H */
