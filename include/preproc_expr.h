/*
 * Expression evaluation used by the preprocessor.
 *
 * Provides `eval_expr` which parses and evaluates the limited boolean
 * expressions permitted in conditional directives.  Evaluation uses the
 * currently defined macros to resolve the `defined` operator.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PREPROC_EXPR_H
#define VC_PREPROC_EXPR_H

#include "vector.h"
#include "preproc_macros.h"

/* Evaluate a conditional expression */
long long eval_expr(const char *s, vector_t *macros);

#endif /* VC_PREPROC_EXPR_H */
