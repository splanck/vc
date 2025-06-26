/*
 * Expression semantic analysis helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_EXPR_H
#define VC_SEMANTIC_EXPR_H

#include "ast.h"
#include "ir.h"
#include "symtable.h"

int is_intlike(type_kind_t t);
int is_floatlike(type_kind_t t);
int eval_const_expr(expr_t *expr, symtable_t *vars, long long *out);
type_kind_t check_expr(expr_t *expr, symtable_t *vars, symtable_t *funcs,
                       ir_builder_t *ir, ir_value_t *out);

#endif /* VC_SEMANTIC_EXPR_H */
