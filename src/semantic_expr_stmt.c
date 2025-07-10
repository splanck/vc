#include "semantic_stmt.h"
#include "semantic_expr.h"
#include "semantic_control.h"
#include "ir_core.h"

static int check_expr_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                           ir_builder_t *ir)
{
    ir_value_t tmp;
    return check_expr(stmt->expr.expr, vars, funcs, ir, &tmp) != TYPE_UNKNOWN;
}

int stmt_expr_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                      label_table_t *labels, ir_builder_t *ir,
                      type_kind_t func_ret_type,
                      const char *break_label,
                      const char *continue_label)
{
    (void)labels; (void)func_ret_type; (void)break_label; (void)continue_label;
    return check_expr_stmt(stmt, vars, funcs, ir);
}
