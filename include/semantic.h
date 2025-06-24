#ifndef VC_SEMANTIC_H
#define VC_SEMANTIC_H

#include "ast.h"
#include "ir.h"
#include "symtable.h"


/* Error handling */
void semantic_set_error(size_t line, size_t column);
void semantic_print_error(const char *msg);

/* Expression/statement checking helpers */
int eval_const_expr(expr_t *expr, int *out);
type_kind_t check_expr(expr_t *expr, symtable_t *vars, symtable_t *funcs,
                       ir_builder_t *ir, ir_value_t *out);
int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
               ir_builder_t *ir, type_kind_t func_ret_type,
               const char *break_label, const char *continue_label);
int check_func(func_t *func, symtable_t *funcs, symtable_t *globals,
               ir_builder_t *ir);
int check_global(stmt_t *decl, symtable_t *globals, ir_builder_t *ir);

#endif /* VC_SEMANTIC_H */
