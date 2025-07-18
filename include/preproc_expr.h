/*
 * Expression evaluation used by the preprocessor.
 *
 * Provides helpers to parse and evaluate the limited boolean expressions
 * permitted in conditional directives.  Evaluation uses the currently
 * defined macros to resolve the `defined` operator.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PREPROC_EXPR_H
#define VC_PREPROC_EXPR_H

#include "vector.h"
#include "preproc_macros.h"

/* Evaluate an expression with include lookup support */
long long eval_expr_full(const char *s, vector_t *macros,
                         const char *dir, const vector_t *incdirs,
                         vector_t *stack);

#endif /* VC_PREPROC_EXPR_H */
