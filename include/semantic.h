/*
 * Semantic analysis routines.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_H
#define VC_SEMANTIC_H

#include "ast.h"
#include "ir.h"
#include "symtable.h"


/* Semantic validation helpers */
/*
 * Each function walks the AST and emits IR while checking types.
 * A non-zero return value indicates success.
 */
type_kind_t check_expr(expr_t *expr, symtable_t *vars, symtable_t *funcs,
                       ir_builder_t *ir, ir_value_t *out);
int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
               void *labels, ir_builder_t *ir, type_kind_t func_ret_type,
               const char *break_label, const char *continue_label);
/* Validate an entire function definition */
int check_func(func_t *func, symtable_t *funcs, symtable_t *globals,
               ir_builder_t *ir);
/* Validate a global variable declaration */
int check_global(stmt_t *decl, symtable_t *globals, ir_builder_t *ir);

#endif /* VC_SEMANTIC_H */
