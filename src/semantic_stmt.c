/*
 * Implementation of statement semantic checking.
 * Handles all statement kinds and emits IR reflecting their
 * control flow and side effects.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_stmt.h"
#include "semantic_loops.h"
#include "semantic_switch.h"
#include "consteval.h"
#include "semantic_expr.h"
#include "semantic_init.h"
#include "semantic_global.h"
#include "ir_global.h"
#include "symtable.h"
#include "util.h"
#include "label.h"
#include "error.h"
#include <limits.h>



/*
 * Remove all symbols added after old_head from the table.  Each entry
 * beyond the saved head is popped off the list and its memory is
 * released, effectively restoring the previous scope.
 */
void symtable_pop_scope(symtable_t *table, symbol_t *old_head)
{
    while (table->head != old_head) {
        symbol_t *sym = table->head;
        table->head = sym->next;
        free(sym->name);
        free(sym->param_types);
        free(sym);
    }
}
/*
 * Validate an enum declaration and register its members.  Constant
 * values are evaluated, enumerators are inserted into the symbol
 * table sequentially and an optional tag is recorded.
 */
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
/*
 * Validate a struct declaration and store layout information.  Member
 * offsets are calculated, the type is registered in the symbol table
 * and the total size is recorded for later use.
 */
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
/*
 * Validate a union declaration and record its size.  Member offsets are
 * calculated and the type is inserted into the symbol table so that
 * later references know the union layout.
 */
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
/*
 * Register a typedef alias in the symbol table.  The alias name and
 * type information are stored so that future declarations can refer to
 * the typedef.
 */
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

/* Helpers for variable initialization */
/*
 * Emit IR for a static array initialized with constant values.  The
 * array contents are provided up front and written directly to the
 * global data section.
 */
static void init_static_array(ir_builder_t *ir, const char *name,
                              const long long *vals, size_t count)
{
    ir_build_glob_array(ir, name, vals, count, 1);
}

/*
 * Store constant values into a dynamic array variable.  Each element is
 * written with either a volatile or normal store depending on the
 * declaration.
 */
static void init_dynamic_array(ir_builder_t *ir, const char *name,
                               const long long *vals, size_t count,
                               int is_volatile)
{
    for (size_t i = 0; i < count; i++) {
        ir_value_t idxv = ir_build_const(ir, (int)i);
        ir_value_t valv = ir_build_const(ir, vals[i]);
        if (is_volatile)
            ir_build_store_idx_vol(ir, name, idxv, valv);
        else
            ir_build_store_idx(ir, name, idxv, valv);
    }
}

/*
 * Store a constant initializer into a struct field.  The address of the
 * field is computed using the provided base pointer and offset and the
 * constant value is written through that pointer.
 */
static void init_struct_member(ir_builder_t *ir, ir_value_t base,
                               size_t off, long long val)
{
    ir_value_t offv = ir_build_const(ir, (int)off);
    ir_value_t addr = ir_build_ptr_add(ir, base, offv, 1);
    ir_value_t valv = ir_build_const(ir, val);
    ir_build_store_ptr(ir, addr, valv);
}
/*
 * Expand an initializer list for an array variable.  The initializer
 * entries are evaluated, converted to constant values and stored either
 * at sequential indices or at explicit positions.  The resulting array
 * is then emitted using the appropriate helper.
 */
static int handle_array_init(stmt_t *stmt, symbol_t *sym, symtable_t *vars, ir_builder_t *ir)
{
    long long *vals;
    if (!expand_array_initializer(stmt->var_decl.init_list, stmt->var_decl.init_count,
                                  sym->array_size, vars, stmt->line, stmt->column,
                                  &vals))
        return 0;
    if (stmt->var_decl.is_static)
        init_static_array(ir, sym->ir_name, vals, sym->array_size);
    else
        init_dynamic_array(ir, sym->ir_name, vals, sym->array_size,
                           stmt->var_decl.is_volatile);
    free(vals);
    return 1;
}
/*
 * Expand an initializer list for a struct variable.  Each provided
 * initializer assigns to the next field or to a named field.  Constant
 * values are stored into the computed member addresses of the struct
 * variable.
 */
static int handle_struct_init(stmt_t *stmt, symbol_t *sym, symtable_t *vars, ir_builder_t *ir)
{
    long long *vals;
    if (!expand_struct_initializer(stmt->var_decl.init_list, stmt->var_decl.init_count,
                                   sym, vars, stmt->line, stmt->column, &vals))
        return 0;
    ir_value_t base = ir_build_addr(ir, sym->ir_name);
    for (size_t i = 0; i < sym->struct_member_count; i++)
        init_struct_member(ir, base, sym->struct_members[i].offset, vals[i]);
    free(vals);
    return 1;
}
/*
 * Validate a variable declaration and generate initialization IR.  The
 * symbol is added to the current scope, any struct or union layout is
 * recorded and the optional initializer is expanded into IR.
 */
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
            if (!handle_array_init(stmt, sym, vars, ir))
                return 0;
        } else if (stmt->var_decl.type == TYPE_STRUCT) {
            if (!handle_struct_init(stmt, sym, vars, ir))
                return 0;
        } else {
            error_set(stmt->line, stmt->column);
            return 0;
        }
    }
    return 1;
}
/*
 * Perform semantic checking and emit IR for an if/else statement.  The
 * condition expression is evaluated and used to branch to either the
 * then or else part.  Both branches are validated and finally control
 * converges at a common end label.
 */
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



/*
 * Validate a single statement.  Depending on the statement kind this
 * routine dispatches to the appropriate helper.  Loop labels provide
 * targets for break and continue statements.  Emits IR as a side
 * effect and returns non-zero on success.
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
