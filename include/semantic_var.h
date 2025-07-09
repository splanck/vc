/*
 * Variable initialization and layout helpers.
 * Provides routines to compute aggregate sizes and emit IR
 * for local variable initialization.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_VAR_H
#define VC_SEMANTIC_VAR_H

#include "ast_stmt.h"
#include "ir_core.h"
#include "symtable.h"

int compute_var_layout(stmt_t *stmt, symtable_t *vars);
int handle_vla_size(stmt_t *stmt, symbol_t *sym, symtable_t *vars,
                    symtable_t *funcs, ir_builder_t *ir);
int emit_var_initializer(stmt_t *stmt, symbol_t *sym,
                         symtable_t *vars, symtable_t *funcs,
                         ir_builder_t *ir);

#endif /* VC_SEMANTIC_VAR_H */
