#include "semantic_stmt.h"
#include "semantic_control.h"
#include "symtable.h"
#include "ir_core.h"

int stmt_block_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                       label_table_t *labels, ir_builder_t *ir,
                       type_kind_t func_ret_type,
                       const char *break_label,
                       const char *continue_label)
{
    symbol_t *old_head = vars->head;
    for (size_t i = 0; i < STMT_BLOCK(stmt).count; i++) {
        if (!check_stmt(STMT_BLOCK(stmt).stmts[i], vars, funcs, labels, ir,
                        func_ret_type, break_label, continue_label)) {
            symtable_pop_scope(vars, old_head);
            return 0;
        }
    }
    symtable_pop_scope(vars, old_head);
    return 1;
}

