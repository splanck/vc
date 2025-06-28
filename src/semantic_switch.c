/*
 * Switch statement checking and label management helpers.
 * Validates switch constructs and manages label mapping while
 * generating the branch IR for cases.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_switch.h"
#include "consteval.h"
#include "semantic_expr.h"
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
    char buf[32];
    e->ir_name = vc_strdup(label_format("Luser", label_next_id(), buf));
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
    size_t count = stmt->switch_stmt.case_count;
    char **labels = calloc(count, sizeof(char *));
    if (!labels)
        return NULL;

    for (size_t i = 0; i < count; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "L%d_case%zu", id, i);
        labels[i] = vc_strdup(buf);
        long long cval;
        if (!eval_const_expr(stmt->switch_stmt.cases[i].expr, vars, &cval)) {
            for (size_t j = 0; j <= i; j++)
                free(labels[j]);
            free(labels);
            error_set(stmt->switch_stmt.cases[i].expr->line,
                      stmt->switch_stmt.cases[i].expr->column);
            return NULL;
        }
        ir_value_t const_val = ir_build_const(ir, cval);
        ir_value_t cmp = ir_build_binop(ir, IR_CMPEQ, expr_val, const_val);
        ir_build_bcond(ir, cmp, labels[i]);
    }

    if (stmt->switch_stmt.default_body)
        ir_build_br(ir, default_label);
    else
        ir_build_br(ir, end_label);

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
    for (size_t i = 0; i < stmt->switch_stmt.case_count; i++) {
        ir_build_label(ir, case_labels[i]);
        if (!check_stmt(stmt->switch_stmt.cases[i].body, vars, funcs, labels,
                        ir, func_ret_type, end_label, NULL))
            return 0;
        ir_build_br(ir, end_label);
    }

    if (stmt->switch_stmt.default_body) {
        ir_build_label(ir, default_label);
        if (!check_stmt(stmt->switch_stmt.default_body, vars, funcs, labels, ir,
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
    if (check_expr(stmt->switch_stmt.expr, vars, funcs, ir, &expr_val) == TYPE_UNKNOWN)
        return 0;
    char end_label[32];
    char default_label[32];
    int id = label_next_id();
    label_format_suffix("L", id, "_end", end_label);
    label_format_suffix("L", id, "_default", default_label);

    /* generate conditional branches to each case */
    char **case_labels = emit_case_branches(stmt, vars, ir, expr_val,
                                           default_label, end_label, id);
    if (!case_labels)
        return 0;

    /* walk the bodies and emit the common end label */
    int ok = process_switch_body(stmt, vars, funcs, labels, ir, func_ret_type,
                                 case_labels, default_label, end_label);

    for (size_t j = 0; j < stmt->switch_stmt.case_count; j++)
        free(case_labels[j]);
    free(case_labels);
    return ok;
}

