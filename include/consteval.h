/*
 * Constant expression evaluation helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_CONSTEVAL_H
#define VC_CONSTEVAL_H

#include "ast.h"
#include "symtable.h"

int is_intlike(type_kind_t t);
int is_floatlike(type_kind_t t);
int eval_const_expr(expr_t *expr, symtable_t *vars, long long *out);

#endif /* VC_CONSTEVAL_H */
