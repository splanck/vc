#include "semantic_stmt.h"
#include "semantic_control.h"
#include "ir_core.h"
#include "consteval.h"
#include "error.h"

static int check_static_assert_stmt(stmt_t *stmt, symtable_t *vars)
{
    long long val;
    if (!eval_const_expr(stmt->data.static_assert.expr, vars, 0, &val)) {
        error_set(stmt->data.static_assert.expr->line, stmt->data.static_assert.expr->column,
                  error_current_file, error_current_function);
        return 0;
    }
    if (val == 0) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        error_print(stmt->data.static_assert.message);
        return 0;
    }
    return 1;
}

int stmt_static_assert_handler(stmt_t *stmt, symtable_t *vars,
                               symtable_t *funcs, label_table_t *labels,
                               ir_builder_t *ir,
                               type_kind_t func_ret_type,
                               const char *break_label,
                               const char *continue_label)
{
    (void)funcs; (void)labels; (void)ir; (void)func_ret_type;
    (void)break_label; (void)continue_label;
    return check_static_assert_stmt(stmt, vars);
}

