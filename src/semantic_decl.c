#include <stdlib.h>
#include <string.h>
#include "semantic_stmt.h"
#include "semantic_var.h"
#include "semantic_layout.h"
#include "consteval.h"
#include "semantic_control.h"
#include "ir_core.h"
#include "label.h"
#include "error.h"

static int check_typedef_stmt(stmt_t *stmt, symtable_t *vars)
{
    if (!symtable_add_typedef(vars, STMT_TYPEDEF(stmt).name,
                              STMT_TYPEDEF(stmt).type,
                              STMT_TYPEDEF(stmt).array_size,
                              STMT_TYPEDEF(stmt).elem_size)) {
        error_set(&error_ctx, stmt->line, stmt->column, NULL, NULL);
        return 0;
    }
    return 1;
}

int stmt_typedef_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                         label_table_t *labels, ir_builder_t *ir,
                         type_kind_t func_ret_type,
                         const char *break_label,
                         const char *continue_label)
{
    (void)funcs; (void)labels; (void)ir; (void)func_ret_type;
    (void)break_label; (void)continue_label;
    return check_typedef_stmt(stmt, vars);
}

