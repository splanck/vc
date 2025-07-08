/*
 * Statement semantic validation helpers.
 * Provides the function used to verify statements and
 * emit IR for their control flow and effects.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_STMT_H
#define VC_SEMANTIC_STMT_H

#include "ast.h"
#include "ir_core.h"
#include "symtable.h"
#include <stdbool.h>

int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
               void *labels, ir_builder_t *ir, type_kind_t func_ret_type,
               const char *break_label, const char *continue_label);

/* Emit warnings for unreachable statements in a function body */
extern bool semantic_warn_unreachable;
void warn_unreachable_function(func_t *func);

#endif /* VC_SEMANTIC_STMT_H */
