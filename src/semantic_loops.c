/*
 * Loop statement semantic helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include "semantic_loops.h"
#include "semantic_expr.h"
#include "label.h"

/* Forward declaration from semantic_stmt.c */
extern int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                      void *labels, ir_builder_t *ir, type_kind_t func_ret_type,
                      const char *break_label, const char *continue_label);
extern void symtable_pop_scope(symtable_t *table, symbol_t *old_head);

/*
 * Perform semantic checks for a while loop.  The condition is
 * evaluated first to decide whether the body should execute.  The
 * function sets up start/end labels, validates the body with the
 * appropriate break and continue targets and finally emits the back
 * edge to the loop start.
 */
int check_while_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                     label_table_t *labels, ir_builder_t *ir,
                     type_kind_t func_ret_type)
{
    ir_value_t cond_val;
    char start_label[32];
    char end_label[32];
    int id = label_next_id();
    label_format_suffix("L", id, "_start", start_label);
    label_format_suffix("L", id, "_end", end_label);
    ir_build_label(ir, start_label);
    if (check_expr(stmt->while_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
        return 0;
    ir_build_bcond(ir, cond_val, end_label);
    if (!check_stmt(stmt->while_stmt.body, vars, funcs, labels, ir,
                    func_ret_type, end_label, start_label))
        return 0;
    ir_build_br(ir, start_label);
    ir_build_label(ir, end_label);
    return 1;
}

/*
 * Perform semantic checks for a do-while loop.  The body executes
 * once before the condition is tested.  This routine sets up labels
 * for the loop start, condition check and end, verifies the body and
 * emits the back edge controlled by the condition expression.
 */
int check_do_while_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                        label_table_t *labels, ir_builder_t *ir,
                        type_kind_t func_ret_type)
{
    ir_value_t cond_val;
    char start_label[32];
    char cond_label[32];
    char end_label[32];
    int id = label_next_id();
    label_format_suffix("L", id, "_start", start_label);
    label_format_suffix("L", id, "_cond", cond_label);
    label_format_suffix("L", id, "_end", end_label);
    ir_build_label(ir, start_label);
    if (!check_stmt(stmt->do_while_stmt.body, vars, funcs, labels, ir,
                    func_ret_type, end_label, cond_label))
        return 0;
    ir_build_label(ir, cond_label);
    if (check_expr(stmt->do_while_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
        return 0;
    ir_build_bcond(ir, cond_val, end_label);
    ir_build_br(ir, start_label);
    ir_build_label(ir, end_label);
    return 1;
}

/*
 * Perform semantic checks for a for loop.  The initializer is
 * processed first, then the loop condition is evaluated before each
 * iteration.  The body is checked with proper break and continue
 * labels and the increment expression is emitted before jumping back
 * to the condition.
 */
int check_for_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                   label_table_t *labels, ir_builder_t *ir,
                   type_kind_t func_ret_type)
{
    ir_value_t cond_val;
    char start_label[32];
    char end_label[32];
    int id = label_next_id();
    label_format_suffix("L", id, "_start", start_label);
    label_format_suffix("L", id, "_end", end_label);
    symbol_t *old_head = vars->head;
    if (stmt->for_stmt.init_decl) {
        if (!check_stmt(stmt->for_stmt.init_decl, vars, funcs, labels, ir,
                        func_ret_type, NULL, NULL)) {
            symtable_pop_scope(vars, old_head);
            return 0;
        }
    } else {
        if (check_expr(stmt->for_stmt.init, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN) {
            symtable_pop_scope(vars, old_head);
            return 0; /* reuse cond_val for init but ignore value */
        }
    }
    ir_build_label(ir, start_label);
    if (check_expr(stmt->for_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN) {
        symtable_pop_scope(vars, old_head);
        return 0;
    }
    ir_build_bcond(ir, cond_val, end_label);
    char cont_label[32];
    label_format_suffix("L", id, "_cont", cont_label);
    if (!check_stmt(stmt->for_stmt.body, vars, funcs, labels, ir,
                    func_ret_type, end_label, cont_label)) {
        symtable_pop_scope(vars, old_head);
        return 0;
    }
    ir_build_label(ir, cont_label);
    if (check_expr(stmt->for_stmt.incr, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN) {
        symtable_pop_scope(vars, old_head);
        return 0;
    }
    ir_build_br(ir, start_label);
    ir_build_label(ir, end_label);
    symtable_pop_scope(vars, old_head);
    return 1;
}

