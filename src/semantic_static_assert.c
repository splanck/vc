#include "semantic_stmt.h"
#include "semantic_control.h"
#include "ir_core.h"
#include "consteval.h"
#include "error.h"

static int check_static_assert_stmt(stmt_t *stmt, symtable_t *vars)
{
    long long val;
    if (!eval_const_expr(STMT_STATIC_ASSERT(stmt).expr, vars,
                         semantic_get_x86_64(), &val)) {
        error_set(STMT_STATIC_ASSERT(stmt).expr->line, STMT_STATIC_ASSERT(stmt).expr->column,
                  error_current_file, error_current_function);
        return 0;
    }
    if (val == 0) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        error_print(STMT_STATIC_ASSERT(stmt).message);
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

