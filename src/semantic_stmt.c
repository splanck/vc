/* Semantic statement validation implementation. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_stmt.h"
#include "semantic_expr.h"
#include "semantic_global.h"
#include "symtable.h"
#include "util.h"
#include "label.h"
#include "error.h"
#include <limits.h>



void label_table_init(label_table_t *t) { t->head = NULL; }

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

const char *label_table_get(label_table_t *t, const char *name)
{
    for (label_entry_t *e = t->head; e; e = e->next) {
        if (strcmp(e->name, name) == 0)
            return e->ir_name;
    }
    return NULL;
}

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

static void symtable_pop_scope(symtable_t *table, symbol_t *old_head)
{
    while (table->head != old_head) {
        symbol_t *sym = table->head;
        table->head = sym->next;
        free(sym->name);
        free(sym->param_types);
        free(sym);
    }
}

static int check_enum_decl_stmt(stmt_t *stmt, symtable_t *vars)
{
    int next = 0;
    for (size_t i = 0; i < stmt->enum_decl.count; i++) {
        enumerator_t *e = &stmt->enum_decl.items[i];
        long long val = next;
        if (e->value) {
            if (!eval_const_expr(e->value, vars, &val)) {
                error_set(e->value->line, e->value->column);
                return 0;
            }
        }
        if (!symtable_add_enum(vars, e->name, (int)val)) {
            error_set(stmt->line, stmt->column);
            return 0;
        }
        next = (int)val + 1;
    }
    if (stmt->enum_decl.tag && stmt->enum_decl.tag[0])
        symtable_add_enum_tag(vars, stmt->enum_decl.tag);
    return 1;
}

static int check_struct_decl_stmt(stmt_t *stmt, symtable_t *vars)
{
    size_t total = layout_struct_members(stmt->struct_decl.members,
                                         stmt->struct_decl.count);
    if (!symtable_add_struct(vars, stmt->struct_decl.tag,
                             stmt->struct_decl.members,
                             stmt->struct_decl.count)) {
        error_set(stmt->line, stmt->column);
        return 0;
    }
    symbol_t *stype = symtable_lookup_struct(vars, stmt->struct_decl.tag);
    if (stype)
        stype->struct_total_size = total;
    return 1;
}

static int check_union_decl_stmt(stmt_t *stmt, symtable_t *vars)
{
    size_t max = layout_union_members(stmt->union_decl.members,
                                      stmt->union_decl.count);
    (void)max;
    if (!symtable_add_union(vars, stmt->union_decl.tag,
                            stmt->union_decl.members,
                            stmt->union_decl.count)) {
        error_set(stmt->line, stmt->column);
        return 0;
    }
    return 1;
}

static int check_typedef_stmt(stmt_t *stmt, symtable_t *vars)
{
    if (!symtable_add_typedef(vars, stmt->typedef_decl.name,
                              stmt->typedef_decl.type,
                              stmt->typedef_decl.array_size,
                              stmt->typedef_decl.elem_size)) {
        error_set(stmt->line, stmt->column);
        return 0;
    }
    return 1;
}

static int check_var_decl_stmt(stmt_t *stmt, symtable_t *vars,
                               symtable_t *funcs, ir_builder_t *ir)
{
    char ir_name_buf[32];
    const char *ir_name = stmt->var_decl.name;
    if (stmt->var_decl.is_static)
        ir_name = label_format("__static", label_next_id(), ir_name_buf);

    if (stmt->var_decl.type == TYPE_UNION) {
        size_t max = layout_union_members(stmt->var_decl.members,
                                         stmt->var_decl.member_count);
        stmt->var_decl.elem_size = max;
    }
    if (stmt->var_decl.type == TYPE_STRUCT) {
        size_t total = layout_struct_members(
            (struct_member_t *)stmt->var_decl.members,
            stmt->var_decl.member_count);
        if (stmt->var_decl.member_count || stmt->var_decl.tag)
            stmt->var_decl.elem_size = total;
    }
    if (stmt->var_decl.is_const && !stmt->var_decl.init &&
        !stmt->var_decl.init_list) {
        error_set(stmt->line, stmt->column);
        return 0;
    }
    if (!symtable_add(vars, stmt->var_decl.name, ir_name,
                      stmt->var_decl.type,
                      stmt->var_decl.array_size,
                      stmt->var_decl.elem_size,
                      stmt->var_decl.is_static,
                      stmt->var_decl.is_register,
                      stmt->var_decl.is_const,
                      stmt->var_decl.is_volatile,
                      stmt->var_decl.is_restrict)) {
        error_set(stmt->line, stmt->column);
        return 0;
    }

    symbol_t *sym = symtable_lookup(vars, stmt->var_decl.name);
    if (stmt->var_decl.init_list && stmt->var_decl.type == TYPE_ARRAY &&
        stmt->var_decl.array_size == 0) {
        stmt->var_decl.array_size = stmt->var_decl.init_count;
        sym->array_size = stmt->var_decl.array_size;
    }

    if (stmt->var_decl.type == TYPE_UNION) {
        sym->total_size = stmt->var_decl.elem_size;
        if (stmt->var_decl.member_count) {
            sym->members = malloc(stmt->var_decl.member_count *
                                  sizeof(*sym->members));
            if (!sym->members)
                return 0;
            sym->member_count = stmt->var_decl.member_count;
            for (size_t i = 0; i < sym->member_count; i++) {
                union_member_t *m = &stmt->var_decl.members[i];
                sym->members[i].name = vc_strdup(m->name);
                sym->members[i].type = m->type;
                sym->members[i].elem_size = m->elem_size;
                sym->members[i].offset = m->offset;
            }
        }
    }

    if (stmt->var_decl.type == TYPE_STRUCT) {
        sym->struct_total_size = stmt->var_decl.elem_size;
        if (stmt->var_decl.member_count) {
            sym->struct_members = malloc(stmt->var_decl.member_count *
                                         sizeof(*sym->struct_members));
            if (!sym->struct_members)
                return 0;
            sym->struct_member_count = stmt->var_decl.member_count;
            for (size_t i = 0; i < sym->struct_member_count; i++) {
                struct_member_t *m =
                    (struct_member_t *)&stmt->var_decl.members[i];
                sym->struct_members[i].name = vc_strdup(m->name);
                sym->struct_members[i].type = m->type;
                sym->struct_members[i].elem_size = m->elem_size;
                sym->struct_members[i].offset = m->offset;
            }
        }
    }

    if (stmt->var_decl.init) {
        if (stmt->var_decl.is_static) {
            long long cval;
            if (!eval_const_expr(stmt->var_decl.init, vars, &cval)) {
                error_set(stmt->var_decl.init->line, stmt->var_decl.init->column);
                return 0;
            }
            if (stmt->var_decl.type == TYPE_UNION)
                ir_build_glob_union(ir, sym->ir_name, (int)sym->elem_size, 1);
            else if (stmt->var_decl.type == TYPE_STRUCT)
                ir_build_glob_struct(ir, sym->ir_name,
                                     (int)sym->struct_total_size, 1);
            else
                ir_build_glob_var(ir, sym->ir_name, cval, 1);
        } else {
            ir_value_t val;
            type_kind_t vt =
                check_expr(stmt->var_decl.init, vars, funcs, ir, &val);
            if (!(((is_intlike(stmt->var_decl.type) && is_intlike(vt)) ||
                   (is_floatlike(stmt->var_decl.type) &&
                    (is_floatlike(vt) || is_intlike(vt)))) ||
                  vt == stmt->var_decl.type)) {
                error_set(stmt->var_decl.init->line, stmt->var_decl.init->column);
                return 0;
            }
            if (stmt->var_decl.is_volatile)
                ir_build_store_vol(ir, sym->ir_name, val);
            else
                ir_build_store(ir, sym->ir_name, val);
        }
    } else if (stmt->var_decl.init_list) {
        if (stmt->var_decl.type == TYPE_ARRAY) {
            if (stmt->var_decl.array_size < stmt->var_decl.init_count) {
                error_set(stmt->line, stmt->column);
                return 0;
            }
            long long *vals = calloc(stmt->var_decl.array_size, sizeof(long long));
            if (!vals)
                return 0;
            size_t cur = 0;
            for (size_t i = 0; i < stmt->var_decl.init_count; i++) {
                init_entry_t *ent = &stmt->var_decl.init_list[i];
                size_t idx = cur;
                if (ent->kind == INIT_INDEX) {
                    long long cidx;
                    if (!eval_const_expr(ent->index, vars, &cidx) ||
                        cidx < 0 || (size_t)cidx >= stmt->var_decl.array_size) {
                        free(vals);
                        error_set(ent->index->line, ent->index->column);
                        return 0;
                    }
                    idx = (size_t)cidx;
                    cur = idx;
                } else if (ent->kind == INIT_FIELD) {
                    free(vals);
                    error_set(stmt->line, stmt->column);
                    return 0;
                }
                long long val;
                if (!eval_const_expr(ent->value, vars, &val)) {
                    free(vals);
                    error_set(ent->value->line, ent->value->column);
                    return 0;
                }
                if (idx >= stmt->var_decl.array_size) {
                    free(vals);
                    error_set(stmt->line, stmt->column);
                    return 0;
                }
                vals[idx] = val;
                cur = idx + 1;
            }
            if (stmt->var_decl.is_static) {
                ir_build_glob_array(ir, sym->ir_name, vals,
                                   stmt->var_decl.array_size, 1);
            } else {
                for (size_t i = 0; i < stmt->var_decl.array_size; i++) {
                    ir_value_t idxv = ir_build_const(ir, (int)i);
                    ir_value_t valv = ir_build_const(ir, vals[i]);
                    if (stmt->var_decl.is_volatile)
                        ir_build_store_idx_vol(ir, sym->ir_name, idxv, valv);
                    else
                        ir_build_store_idx(ir, sym->ir_name, idxv, valv);
                }
            }
            free(vals);
        } else if (stmt->var_decl.type == TYPE_STRUCT) {
            if (!sym->struct_member_count) {
                error_set(stmt->line, stmt->column);
                return 0;
            }
            ir_value_t base = ir_build_addr(ir, sym->ir_name);
            size_t cur = 0;
            for (size_t i = 0; i < stmt->var_decl.init_count; i++) {
                init_entry_t *ent = &stmt->var_decl.init_list[i];
                size_t off = 0;
                size_t mi = cur;
                if (ent->kind == INIT_FIELD) {
                    int found = 0;
                    for (size_t j = 0; j < sym->struct_member_count; j++) {
                        if (strcmp(sym->struct_members[j].name, ent->field) == 0) {
                            off = sym->struct_members[j].offset;
                            mi = j;
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        error_set(stmt->line, stmt->column);
                        return 0;
                    }
                    cur = mi;
                } else if (ent->kind == INIT_SIMPLE) {
                    if (cur >= sym->struct_member_count) {
                        error_set(stmt->line, stmt->column);
                        return 0;
                    }
                    off = sym->struct_members[cur].offset;
                } else {
                    error_set(stmt->line, stmt->column);
                    return 0;
                }
                long long val;
                if (!eval_const_expr(ent->value, vars, &val)) {
                    error_set(ent->value->line, ent->value->column);
                    return 0;
                }
                ir_value_t offv = ir_build_const(ir, (int)off);
                ir_value_t addr = ir_build_ptr_add(ir, base, offv, 1);
                ir_value_t valv = ir_build_const(ir, val);
                ir_build_store_ptr(ir, addr, valv);
                cur = mi + 1;
            }
        } else {
            error_set(stmt->line, stmt->column);
            return 0;
        }
    }
    return 1;
}
static int check_if_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                         label_table_t *labels, ir_builder_t *ir,
                         type_kind_t func_ret_type,
                         const char *break_label, const char *continue_label)
{
    ir_value_t cond_val;
    if (check_expr(stmt->if_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
        return 0;
    char else_label[32];
    char end_label[32];
    int id = label_next_id();
    label_format_suffix("L", id, "_else", else_label);
    label_format_suffix("L", id, "_end", end_label);
    const char *target = stmt->if_stmt.else_branch ? else_label : end_label;
    ir_build_bcond(ir, cond_val, target);
    if (!check_stmt(stmt->if_stmt.then_branch, vars, funcs, labels, ir,
                    func_ret_type, break_label, continue_label))
        return 0;
    if (stmt->if_stmt.else_branch) {
        ir_build_br(ir, end_label);
        ir_build_label(ir, else_label);
        if (!check_stmt(stmt->if_stmt.else_branch, vars, funcs, labels, ir,
                        func_ret_type, break_label, continue_label))
            return 0;
    }
    ir_build_label(ir, end_label);
    return 1;
}

static int check_while_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
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

static int check_do_while_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
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

static int check_for_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
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

static int check_switch_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
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
    char **case_labels = calloc(stmt->switch_stmt.case_count, sizeof(char *));
    if (!case_labels)
        return 0;
    for (size_t i = 0; i < stmt->switch_stmt.case_count; i++) {
        char lbl[32];
        snprintf(lbl, sizeof(lbl), "L%d_case%zu", id, i);
        case_labels[i] = vc_strdup(lbl);
        long long cval;
        if (!eval_const_expr(stmt->switch_stmt.cases[i].expr, vars, &cval)) {
            for (size_t j = 0; j <= i; j++) free(case_labels[j]);
            free(case_labels);
            error_set(stmt->switch_stmt.cases[i].expr->line,
                      stmt->switch_stmt.cases[i].expr->column);
            return 0;
        }
        ir_value_t const_val = ir_build_const(ir, cval);
        ir_value_t cmp = ir_build_binop(ir, IR_CMPEQ, expr_val, const_val);
        ir_build_bcond(ir, cmp, case_labels[i]);
    }
    if (stmt->switch_stmt.default_body)
        ir_build_br(ir, default_label);
    else
        ir_build_br(ir, end_label);
    for (size_t i = 0; i < stmt->switch_stmt.case_count; i++) {
        ir_build_label(ir, case_labels[i]);
        if (!check_stmt(stmt->switch_stmt.cases[i].body, vars, funcs, labels, ir,
                        func_ret_type, end_label, NULL)) {
            for (size_t j = 0; j < stmt->switch_stmt.case_count; j++)
                free(case_labels[j]);
            free(case_labels);
            return 0;
        }
        ir_build_br(ir, end_label);
    }
    if (stmt->switch_stmt.default_body) {
        ir_build_label(ir, default_label);
        if (!check_stmt(stmt->switch_stmt.default_body, vars, funcs, labels, ir,
                        func_ret_type, end_label, NULL)) {
            for (size_t j = 0; j < stmt->switch_stmt.case_count; j++)
                free(case_labels[j]);
            free(case_labels);
            return 0;
        }
    }
    ir_build_label(ir, end_label);
    for (size_t j = 0; j < stmt->switch_stmt.case_count; j++)
        free(case_labels[j]);
    free(case_labels);
    return 1;
}

/*
 * Validate a single statement.  Loop labels are used for 'break' and
 * 'continue' targets.  Returns non-zero on success.
 */
int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
               void *label_tab, ir_builder_t *ir, type_kind_t func_ret_type,
               const char *break_label, const char *continue_label)
{
    label_table_t *labels = (label_table_t *)label_tab;
    if (!stmt)
        return 0;
    switch (stmt->kind) {
    case STMT_EXPR: {
        ir_value_t tmp;
        return check_expr(stmt->expr.expr, vars, funcs, ir, &tmp) != TYPE_UNKNOWN;
    }
    case STMT_RETURN: {
        if (!stmt->ret.expr) {
            if (func_ret_type != TYPE_VOID) {
                error_set(stmt->line, stmt->column);
                return 0;
            }
            ir_value_t zero = ir_build_const(ir, 0);
            ir_build_return(ir, zero);
            return 1;
        }
        ir_value_t val;
        if (check_expr(stmt->ret.expr, vars, funcs, ir, &val) == TYPE_UNKNOWN) {
            error_set(stmt->ret.expr->line, stmt->ret.expr->column);
            return 0;
        }
        ir_build_return(ir, val);
        return 1;
    }
    case STMT_IF:
        return check_if_stmt(stmt, vars, funcs, labels, ir, func_ret_type,
                             break_label, continue_label);
    case STMT_WHILE:
        return check_while_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
    case STMT_DO_WHILE:
        return check_do_while_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
    case STMT_FOR:
        return check_for_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
    case STMT_SWITCH:
        return check_switch_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
    case STMT_LABEL: {
        const char *ir_name = label_table_get_or_add(labels, stmt->label.name);
        ir_build_label(ir, ir_name);
        return 1;
    }
    case STMT_GOTO: {
        const char *ir_name = label_table_get_or_add(labels, stmt->goto_stmt.name);
        ir_build_br(ir, ir_name);
        return 1;
    }
    case STMT_BREAK:
        if (!break_label) {
            error_set(stmt->line, stmt->column);
            return 0;
        }
        ir_build_br(ir, break_label);
        return 1;
    case STMT_CONTINUE:
        if (!continue_label) {
            error_set(stmt->line, stmt->column);
            return 0;
        }
        ir_build_br(ir, continue_label);
        return 1;
    case STMT_BLOCK: {
        symbol_t *old_head = vars->head;
        for (size_t i = 0; i < stmt->block.count; i++) {
            if (!check_stmt(stmt->block.stmts[i], vars, funcs, labels, ir, func_ret_type,
                            break_label, continue_label)) {
                symtable_pop_scope(vars, old_head);
                return 0;
            }
        }
        symtable_pop_scope(vars, old_head);
        return 1;
    }
    case STMT_ENUM_DECL:
        return check_enum_decl_stmt(stmt, vars);
    case STMT_STRUCT_DECL:
        return check_struct_decl_stmt(stmt, vars);
    case STMT_UNION_DECL:
        return check_union_decl_stmt(stmt, vars);
    case STMT_TYPEDEF:
        return check_typedef_stmt(stmt, vars);
    case STMT_VAR_DECL:
        return check_var_decl_stmt(stmt, vars, funcs, ir);
    }
    return 0;
}
