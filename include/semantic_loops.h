/*
 * Loop statement semantic helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_LOOPS_H
#define VC_SEMANTIC_LOOPS_H

#include "ast.h"
#include "ir.h"
#include "symtable.h"
#include "semantic_switch.h"

int check_while_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                     label_table_t *labels, ir_builder_t *ir,
                     type_kind_t func_ret_type);
int check_do_while_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                        label_table_t *labels, ir_builder_t *ir,
                        type_kind_t func_ret_type);
int check_for_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                   label_table_t *labels, ir_builder_t *ir,
                   type_kind_t func_ret_type);

#endif /* VC_SEMANTIC_LOOPS_H */
