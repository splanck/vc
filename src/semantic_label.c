#include "semantic_stmt.h"
#include "semantic_control.h"
#include "ir_core.h"
#include "error.h"

static int handle_label_stmt(stmt_t *stmt, label_table_t *labels,
                             ir_builder_t *ir)
{
    const char *ir_name = label_table_get_or_add(labels, stmt->label.name);
    ir_build_label(ir, ir_name);
    return 1;
}

static int check_goto_stmt(stmt_t *stmt, label_table_t *labels,
                           ir_builder_t *ir)
{
    const char *ir_name = label_table_get_or_add(labels, stmt->goto_stmt.name);
    ir_build_br(ir, ir_name);
    return 1;
}

int stmt_label_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                       label_table_t *labels, ir_builder_t *ir,
                       type_kind_t func_ret_type,
                       const char *break_label,
                       const char *continue_label)
{
    (void)vars; (void)funcs; (void)func_ret_type;
    (void)break_label; (void)continue_label;
    return handle_label_stmt(stmt, labels, ir);
}

int stmt_goto_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                      label_table_t *labels, ir_builder_t *ir,
                      type_kind_t func_ret_type,
                      const char *break_label,
                      const char *continue_label)
{
    (void)vars; (void)funcs; (void)func_ret_type;
    (void)break_label; (void)continue_label;
    return check_goto_stmt(stmt, labels, ir);
}

