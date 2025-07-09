/*
 * Loop statement semantic helpers.
 * Functions here check loop constructs and emit the branches
 * required to implement them in IR.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_LOOPS_H
#define VC_SEMANTIC_LOOPS_H

#include "ast_stmt.h"
#include "ir_core.h"
#include "symtable.h"
#include "semantic_switch.h"

/* Check a while loop and emit IR */
int check_while_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                     label_table_t *labels, ir_builder_t *ir,
                     type_kind_t func_ret_type);

/* Check a do-while loop and emit IR */
int check_do_while_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                        label_table_t *labels, ir_builder_t *ir,
                        type_kind_t func_ret_type);

/* Check a for loop and emit IR */
int check_for_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                   label_table_t *labels, ir_builder_t *ir,
                   type_kind_t func_ret_type);

/* Validate a break statement */
int check_break_stmt(stmt_t *stmt, const char *break_label, ir_builder_t *ir);

/* Validate a continue statement */
int check_continue_stmt(stmt_t *stmt, const char *continue_label,
                        ir_builder_t *ir);

#endif /* VC_SEMANTIC_LOOPS_H */
