/*
 * Control flow statement helpers.
 * Validates if/else and switch constructs and manages label mapping while
 * generating the branch IR needed for control flow.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_control.h"
#include "consteval.h"
#include "semantic_expr.h"
#include "semantic.h"
#include "util.h"
#include "label.h"
#include "error.h"

/* Forward declaration from semantic_stmt.c */
extern int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                      void *labels, ir_builder_t *ir, type_kind_t func_ret_type,
                      const char *break_label, const char *continue_label);

/* Helpers for switch statement IR generation */
static char **emit_case_branches(stmt_t *stmt, symtable_t *vars,
                                 ir_builder_t *ir, ir_value_t expr_val,
                                 const char *default_label,
                                 const char *end_label, int id);
static int process_switch_body(stmt_t *stmt, symtable_t *vars,
                               symtable_t *funcs, label_table_t *labels,
                               ir_builder_t *ir, type_kind_t func_ret_type,
                               char **case_labels,
                               const char *default_label,
                               const char *end_label);

/*
 * Initialize an empty label table used to map user labels to IR
 * labels.  Simply sets the head pointer to NULL so entries can be
 * added later.
 */
void label_table_init(label_table_t *t) { t->head = NULL; }

/*
 * Free all memory used by a label table.  Each entry is removed from
 * the linked list, its strings are released and finally the table head
 * is cleared.
 */
void label_table_free(label_table_t *t)
{
    label_entry_t *e = t->head;
    while (e) {
        label_entry_t *n = e->next;
        free(e->name);
        free(e->ir_name);
        free(e);
        e = n;
    }
    t->head = NULL;
}

/*
 * Look up a label by its user-facing name and return the associated IR
 * name.  If the label does not exist, NULL is returned.
 */
const char *label_table_get(label_table_t *t, const char *name)
{
    for (label_entry_t *e = t->head; e; e = e->next) {
        if (strcmp(e->name, name) == 0)
            return e->ir_name;
    }
    return NULL;
}

/*
 * Retrieve the IR name for a user label, creating a new entry if one
 * does not already exist.  Newly created labels are given a unique IR
 * identifier and inserted at the head of the list.
 */
const char *label_table_get_or_add(label_table_t *t, const char *name)
{
    const char *ir = label_table_get(t, name);
    if (ir)
        return ir;
    label_entry_t *e = malloc(sizeof(*e));
    if (!e)
        return NULL;
    e->name = vc_strdup(name);
    if (!e->name) {
        free(e);
        return NULL;
    }
    char buf[32];
    const char *fmt = label_format("Luser", label_next_id(), buf);
    if (!fmt) {
        free(e->name);
        free(e);
        return NULL;
    }
    e->ir_name = vc_strdup(fmt);
    if (!e->ir_name) {
        free(e->name);
        free(e);
        return NULL;
    }
    e->next = t->head;
    t->head = e;
    return e->ir_name;
}

/*
 * Emit the conditional branches for each case value.  A unique label is
 * generated for every case and the controlling expression is compared
 * against the constant case expression.  If a comparison matches the
 * corresponding case label becomes the branch target.  After all cases
 * are emitted, control falls through to either the default label or the
 * common end label when no default is present.
 */
static char **emit_case_branches(stmt_t *stmt, symtable_t *vars,
                                 ir_builder_t *ir, ir_value_t expr_val,
                                 const char *default_label,
                                 const char *end_label, int id)
{
    size_t count = STMT_SWITCH(stmt).case_count;
    char **labels = calloc(count, sizeof(char *));
    long long *values = calloc(count, sizeof(long long));
    if (!labels || !values) {
        free(labels);
        free(values);
        return NULL;
    }

    for (size_t i = 0; i < count; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "L%d_case%zu", id, i);
        labels[i] = vc_strdup(buf);
        if (!labels[i]) {
            for (size_t j = 0; j < i; j++)
                free(labels[j]);
            free(labels);
            free(values);
            return NULL;
        }
        long long cval;
        if (!eval_const_expr(STMT_SWITCH(stmt).cases[i].expr, vars,
                             semantic_get_x86_64(), &cval)) {
            for (size_t j = 0; j <= i; j++)
                free(labels[j]);
            free(labels);
            free(values);
            error_set(STMT_SWITCH(stmt).cases[i].expr->line, STMT_SWITCH(stmt).cases[i].expr->column, error_current_file, error_current_function);
            return NULL;
        }
        for (size_t j = 0; j < i; j++) {
            if (values[j] == cval) {
                for (size_t k = 0; k <= i; k++)
                    free(labels[k]);
                free(labels);
                free(values);
                error_set(STMT_SWITCH(stmt).cases[i].expr->line, STMT_SWITCH(stmt).cases[i].expr->column, error_current_file, error_current_function);
                return NULL;
            }
        }
        values[i] = cval;
        ir_value_t const_val = ir_build_const(ir, cval);
        ir_value_t cmp = ir_build_binop(ir, IR_CMPEQ, expr_val, const_val, TYPE_INT);
        ir_build_bcond(ir, cmp, labels[i]);
    }

    if (STMT_SWITCH(stmt).default_body)
        ir_build_br(ir, default_label);
    else
        ir_build_br(ir, end_label);

    free(values);
    return labels;
}

/*
 * Walk the case bodies and optional default body of a switch statement.
 * Each case label is placed before its body and execution jumps to the
 * common end label once a case completes.  The default body executes if
 * provided after all cases have been processed.
 */
static int process_switch_body(stmt_t *stmt, symtable_t *vars,
                               symtable_t *funcs, label_table_t *labels,
                               ir_builder_t *ir, type_kind_t func_ret_type,
                               char **case_labels,
                               const char *default_label,
                               const char *end_label)
{
    for (size_t i = 0; i < STMT_SWITCH(stmt).case_count; i++) {
        ir_build_label(ir, case_labels[i]);
        if (!check_stmt(STMT_SWITCH(stmt).cases[i].body, vars, funcs, labels,
                        ir, func_ret_type, end_label, NULL))
            return 0;
        ir_build_br(ir, end_label);
    }

    if (STMT_SWITCH(stmt).default_body) {
        ir_build_label(ir, default_label);
        if (!check_stmt(STMT_SWITCH(stmt).default_body, vars, funcs, labels, ir,
                        func_ret_type, end_label, NULL))
            return 0;
    }

    ir_build_label(ir, end_label);
    return 1;
}

/*
 * Validate a switch statement and emit the corresponding IR.  The
 * controlling expression is evaluated once, each case value is
 * compared against it and branches are created to the generated case
 * labels.  After processing all case bodies and the optional default,
 * control flows to a common end label.
 */
int check_switch_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                      label_table_t *labels, ir_builder_t *ir,
                      type_kind_t func_ret_type)
{
    ir_value_t expr_val;
    if (check_expr(STMT_SWITCH(stmt).expr, vars, funcs, ir, &expr_val) == TYPE_UNKNOWN)
        return 0;
    char end_label[32];
    char default_label[32];
    int id = label_next_id();
    if (!label_format_suffix("L", id, "_end", end_label) ||
        !label_format_suffix("L", id, "_default", default_label))
        return 0;

    /* generate conditional branches to each case */
    char **case_labels = emit_case_branches(stmt, vars, ir, expr_val,
                                           default_label, end_label, id);
    if (!case_labels)
        return 0;

    /* walk the bodies and emit the common end label */
    int ok = process_switch_body(stmt, vars, funcs, labels, ir, func_ret_type,
                                 case_labels, default_label, end_label);

    for (size_t j = 0; j < STMT_SWITCH(stmt).case_count; j++)
        free(case_labels[j]);
    free(case_labels);
    return ok;
}

int stmt_if_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                    label_table_t *labels, ir_builder_t *ir,
                    type_kind_t func_ret_type,
                    const char *break_label, const char *continue_label)
{
    return check_if_stmt(stmt, vars, funcs, labels, ir, func_ret_type,
                         break_label, continue_label);
}

int stmt_switch_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                        label_table_t *labels, ir_builder_t *ir,
                        type_kind_t func_ret_type,
                        const char *break_label, const char *continue_label)
{
    (void)break_label; (void)continue_label;
    return check_switch_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
}

/*
 * Validate an if/else statement and emit the corresponding IR.
 * The condition is evaluated once to decide which branch executes and
 * both branches are checked before control joins at the common end label.
 */
int check_if_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                  label_table_t *labels, ir_builder_t *ir,
                  type_kind_t func_ret_type,
                  const char *break_label, const char *continue_label)
{
    ir_value_t cond_val;
    if (check_expr(STMT_IF(stmt).cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
        return 0;
    char else_label[32];
    char end_label[32];
    int id = label_next_id();
    if (!label_format_suffix("L", id, "_else", else_label) ||
        !label_format_suffix("L", id, "_end", end_label))
        return 0;
    const char *target = STMT_IF(stmt).else_branch ? else_label : end_label;
    ir_build_bcond(ir, cond_val, target);
    if (!check_stmt(STMT_IF(stmt).then_branch, vars, funcs, labels, ir,
                    func_ret_type, break_label, continue_label))
        return 0;
    if (STMT_IF(stmt).else_branch) {
        ir_build_br(ir, end_label);
        ir_build_label(ir, else_label);
        if (!check_stmt(STMT_IF(stmt).else_branch, vars, funcs, labels, ir,
                        func_ret_type, break_label, continue_label))
            return 0;
    }
    ir_build_label(ir, end_label);
    return 1;
}

