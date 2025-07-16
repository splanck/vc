/*
 * Arithmetic expression semantic helpers.
 * Functions verify arithmetic and comparison expressions and
 * produce the corresponding IR instructions.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_EXPR_OPS_H
#define VC_SEMANTIC_EXPR_OPS_H

#include "ast_expr.h"
#include "ir_core.h"
#include "symtable.h"

/* Binary operation helper used by check_binary_expr */
type_kind_t check_binary(expr_t *left, expr_t *right, symtable_t *vars,
                         symtable_t *funcs, ir_builder_t *ir,
                         ir_value_t *out, binop_t op);

type_kind_t check_unary_expr(expr_t *expr, symtable_t *vars,
                             symtable_t *funcs, ir_builder_t *ir,
                             ir_value_t *out);

type_kind_t check_binary_expr(expr_t *expr, symtable_t *vars,
                              symtable_t *funcs, ir_builder_t *ir,
                              ir_value_t *out);

/* Cast expressions */
type_kind_t check_cast_expr(expr_t *expr, symtable_t *vars,
                            symtable_t *funcs, ir_builder_t *ir,
                            ir_value_t *out);

/* Helper returning the IR op for a binary operator */
ir_op_t ir_op_for_binop(binop_t op);

#endif /* VC_SEMANTIC_EXPR_OPS_H */
