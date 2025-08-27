/*
 * Loop statement semantic helpers.
 * These functions verify loop constructs and generate the
 * control-flow IR needed for iteration.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include "semantic_loops.h"
#include "semantic_expr.h"
#include "label.h"
#include "error.h"
#include <assert.h>
#include <string.h>

/* Forward declaration from semantic_stmt.c */
extern int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                      void *labels, ir_builder_t *ir, type_kind_t func_ret_type,
                      const char *break_label, const char *continue_label);

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
    if (!label_format_suffix("L", id, "_start", start_label) ||
        !label_format_suffix("L", id, "_end", end_label))
        return 0;
    ir_build_label(ir, start_label);
    if (check_expr(STMT_WHILE(stmt).cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
        return 0;
    cond_val = ir_build_binop(ir, IR_CMPNE, cond_val, ir_build_const(ir, 0), TYPE_INT);
    ir_build_bcond(ir, cond_val, end_label);
    if (!check_stmt(STMT_WHILE(stmt).body, vars, funcs, labels, ir,
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
    if (!label_format_suffix("L", id, "_start", start_label) ||
        !label_format_suffix("L", id, "_cond", cond_label) ||
        !label_format_suffix("L", id, "_end", end_label))
        return 0;
    ir_build_label(ir, start_label);
    if (!check_stmt(STMT_DO_WHILE(stmt).body, vars, funcs, labels, ir,
                    func_ret_type, end_label, cond_label))
        return 0;
    ir_build_label(ir, cond_label);
    if (check_expr(STMT_DO_WHILE(stmt).cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
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
    if (!label_format_suffix("L", id, "_start", start_label) ||
        !label_format_suffix("L", id, "_end", end_label))
        return 0;
    symbol_t *old_head = vars->head;
    if (STMT_FOR(stmt).init_decl) {
        if (!check_stmt(STMT_FOR(stmt).init_decl, vars, funcs, labels, ir,
                        func_ret_type, NULL, NULL)) {
            symtable_pop_scope(vars, old_head);
            return 0;
        }
    } else {
        if (check_expr(STMT_FOR(stmt).init, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN) {
            symtable_pop_scope(vars, old_head);
            return 0; /* reuse cond_val for init but ignore value */
        }
    }
    ir_instr_t *init_tail = ir->tail;
    ir_build_label(ir, start_label);
    assert(init_tail ? init_tail->next == ir->tail : ir->head == ir->tail);
    if (check_expr(STMT_FOR(stmt).cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN) {
        symtable_pop_scope(vars, old_head);
        return 0;
    }
    ir_instr_t *cond_tail = ir->tail;
    ir_build_bcond(ir, cond_val, end_label);
    assert(cond_tail->next == ir->tail && ir->tail->op == IR_BCOND);
    char cont_label[32];
    if (!label_format_suffix("L", id, "_cont", cont_label)) {
        symtable_pop_scope(vars, old_head);
        return 0;
    }
    if (!check_stmt(STMT_FOR(stmt).body, vars, funcs, labels, ir,
                    func_ret_type, end_label, cont_label)) {
        symtable_pop_scope(vars, old_head);
        return 0;
    }
    ir_instr_t *body_tail = ir->tail;
    ir_build_label(ir, cont_label);
    assert(body_tail->next == ir->tail && ir->tail->op == IR_LABEL &&
           strcmp(ir->tail->name, cont_label) == 0);
    if (check_expr(STMT_FOR(stmt).incr, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN) {
        symtable_pop_scope(vars, old_head);
        return 0;
    }
    ir_instr_t *incr_tail = ir->tail;
    ir_build_br(ir, start_label);
    assert(incr_tail->next == ir->tail && ir->tail->op == IR_BR);
    ir_build_label(ir, end_label);
    assert(ir->tail->op == IR_LABEL && strcmp(ir->tail->name, end_label) == 0);
    symtable_pop_scope(vars, old_head);
    return 1;
}

/* Internal helper used by break/continue handlers */
static int handle_loop_stmt(stmt_t *stmt, const char *target,
                            ir_builder_t *ir)
{
    if (!target) {
        error_set(stmt->line, stmt->column, error_current_file,
                  error_current_function);
        return 0;
    }
    ir_build_br(ir, target);
    return 1;
}

/* Validate a break statement */
int check_break_stmt(stmt_t *stmt, const char *break_label, ir_builder_t *ir)
{
    return handle_loop_stmt(stmt, break_label, ir);
}

/* Validate a continue statement */
int check_continue_stmt(stmt_t *stmt, const char *continue_label,
                        ir_builder_t *ir)
{
    return handle_loop_stmt(stmt, continue_label, ir);
}

int stmt_while_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                       label_table_t *labels, ir_builder_t *ir,
                       type_kind_t func_ret_type,
                       const char *break_label,
                       const char *continue_label)
{
    (void)break_label; (void)continue_label;
    return check_while_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
}

int stmt_do_while_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                          label_table_t *labels, ir_builder_t *ir,
                          type_kind_t func_ret_type,
                          const char *break_label,
                          const char *continue_label)
{
    (void)break_label; (void)continue_label;
    return check_do_while_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
}

int stmt_for_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                     label_table_t *labels, ir_builder_t *ir,
                     type_kind_t func_ret_type,
                     const char *break_label,
                     const char *continue_label)
{
    (void)break_label; (void)continue_label;
    return check_for_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
}

int stmt_break_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                       label_table_t *labels, ir_builder_t *ir,
                       type_kind_t func_ret_type,
                       const char *break_label,
                       const char *continue_label)
{
    (void)vars; (void)funcs; (void)labels; (void)ir; (void)func_ret_type;
    (void)continue_label;
    return check_break_stmt(stmt, break_label, ir);
}

int stmt_continue_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                          label_table_t *labels, ir_builder_t *ir,
                          type_kind_t func_ret_type,
                          const char *break_label,
                          const char *continue_label)
{
    (void)vars; (void)funcs; (void)labels; (void)ir; (void)func_ret_type;
    (void)break_label;
    return check_continue_stmt(stmt, continue_label, ir);
}

