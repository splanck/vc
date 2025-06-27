/*
 * Memory-related expression semantic helpers.
 * Interfaces for checking array indexing, member access and
 * compound literals while emitting the appropriate IR.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_MEM_H
#define VC_SEMANTIC_MEM_H

#include "ast.h"
#include "ir_core.h"
#include "symtable.h"

/* Array indexing */
type_kind_t check_index_expr(expr_t *expr, symtable_t *vars,
                             symtable_t *funcs, ir_builder_t *ir,
                             ir_value_t *out);

type_kind_t check_assign_index_expr(expr_t *expr, symtable_t *vars,
                                    symtable_t *funcs, ir_builder_t *ir,
                                    ir_value_t *out);

/* Struct/union members */
type_kind_t check_assign_member_expr(expr_t *expr, symtable_t *vars,
                                     symtable_t *funcs, ir_builder_t *ir,
                                     ir_value_t *out);

type_kind_t check_member_expr(expr_t *expr, symtable_t *vars,
                               symtable_t *funcs, ir_builder_t *ir,
                               ir_value_t *out);

/* Compound literals */
type_kind_t check_complit_expr(expr_t *expr, symtable_t *vars,
                               symtable_t *funcs, ir_builder_t *ir,
                               ir_value_t *out);

#endif /* VC_SEMANTIC_MEM_H */
