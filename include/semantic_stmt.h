/*
 * Statement semantic validation helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_STMT_H
#define VC_SEMANTIC_STMT_H

#include "ast.h"
#include "ir_core.h"
#include "symtable.h"

int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
               void *labels, ir_builder_t *ir, type_kind_t func_ret_type,
               const char *break_label, const char *continue_label);

#endif /* VC_SEMANTIC_STMT_H */
