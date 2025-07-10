/*
 * Declaration statement semantic helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_DECL_STMT_H
#define VC_SEMANTIC_DECL_STMT_H

#include "ast_stmt.h"
#include "ir_core.h"
#include "symtable.h"
#include "semantic_control.h"

int stmt_enum_decl_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                          label_table_t *labels, ir_builder_t *ir,
                          type_kind_t func_ret_type,
                          const char *break_label,
                          const char *continue_label);

int stmt_struct_decl_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                             label_table_t *labels, ir_builder_t *ir,
                             type_kind_t func_ret_type,
                             const char *break_label,
                             const char *continue_label);

int stmt_union_decl_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                            label_table_t *labels, ir_builder_t *ir,
                            type_kind_t func_ret_type,
                            const char *break_label,
                            const char *continue_label);

int stmt_var_decl_handler(stmt_t *stmt, symtable_t *vars,
                         symtable_t *funcs, label_table_t *labels,
                         ir_builder_t *ir, type_kind_t func_ret_type,
                         const char *break_label,
                         const char *continue_label);

#endif /* VC_SEMANTIC_DECL_STMT_H */
