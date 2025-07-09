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
#include "semantic_var.h"
#include "semantic_global.h"
#include "ir_global.h"
#include "symtable.h"
#include "util.h"
#include "label.h"
#include "error.h"
#include <limits.h>

bool semantic_warn_unreachable = true;



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
            if (!eval_const_expr(e->value, vars, 0, &val)) {
                error_set(e->value->line, e->value->column, error_current_file, error_current_function);
                return 0;
            }
        }
        if (!symtable_add_enum(vars, e->name, (int)val)) {
            error_set(stmt->line, stmt->column, error_current_file, error_current_function);
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
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
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
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
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
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        return 0;
    }
    return 1;
}


/*
 * Insert the variable into the given symbol table using the provided
 * IR name.  If successful the inserted symbol is returned, otherwise
 * NULL is returned.
 */
static symbol_t *insert_var_symbol(stmt_t *stmt, symtable_t *vars,
                                   const char *ir_name)
{
    if (stmt->var_decl.align_expr) {
        long long aval;
        if (!eval_const_expr(stmt->var_decl.align_expr, vars, 0, &aval) ||
            aval <= 0 || (aval & (aval - 1))) {
            error_set(stmt->var_decl.align_expr->line,
                      stmt->var_decl.align_expr->column,
                      error_current_file, error_current_function);
            error_print("Invalid alignment");
            return NULL;
        }
        stmt->var_decl.alignment = (size_t)aval;
    }
    if (!symtable_add(vars, stmt->var_decl.name, ir_name,
                      stmt->var_decl.type,
                      stmt->var_decl.array_size,
                      stmt->var_decl.elem_size,
                      stmt->var_decl.alignment,
                      stmt->var_decl.is_static,
                      stmt->var_decl.is_register,
                      stmt->var_decl.is_const,
                      stmt->var_decl.is_volatile,
                      stmt->var_decl.is_restrict))
        return NULL;

    symbol_t *sym = symtable_lookup(vars, stmt->var_decl.name);
    if (stmt->var_decl.init_list && stmt->var_decl.type == TYPE_ARRAY &&
        stmt->var_decl.array_size == 0) {
        stmt->var_decl.array_size = stmt->var_decl.init_count;
        sym->array_size = stmt->var_decl.array_size;
    }

    return sym;
}

/*
 * Register the variable symbol in the current scope.
 * Handles static name mangling, computes layout information for
 * aggregate types and inserts the symbol into the table.
 * Returns the inserted symbol or NULL on failure.
 */
static symbol_t *register_var_symbol(stmt_t *stmt, symtable_t *vars)
{
    char ir_name_buf[32];
    const char *ir_name = stmt->var_decl.name;
    if (stmt->var_decl.is_static) {
        if (!label_format("__static", label_next_id(), ir_name_buf))
            return NULL;
        ir_name = ir_name_buf;
    }

    if (!compute_var_layout(stmt, vars))
        return NULL;
    if (stmt->var_decl.is_const && !stmt->var_decl.init &&
        !stmt->var_decl.init_list) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        return NULL;
    }
    symbol_t *sym = insert_var_symbol(stmt, vars, ir_name);
    if (!sym) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        return NULL;
    }

    sym->func_ret_type = stmt->var_decl.func_ret_type;
    sym->func_param_count = stmt->var_decl.func_param_count;
    sym->func_variadic = stmt->var_decl.func_variadic;
    if (stmt->var_decl.func_param_count) {
        sym->func_param_types = malloc(sym->func_param_count * sizeof(type_kind_t));
        if (!sym->func_param_types)
            return NULL;
        for (size_t i = 0; i < sym->func_param_count; i++)
            sym->func_param_types[i] = stmt->var_decl.func_param_types[i];
    }

    return sym;
}

/*
 * Copy union member metadata from the parsed declaration to the symbol.
 * Allocates new member arrays and duplicates names. Returns non-zero on
 * success.
 */
static int check_var_decl_stmt(stmt_t *stmt, symtable_t *vars,
                               symtable_t *funcs, ir_builder_t *ir)
{
    symbol_t *sym = register_var_symbol(stmt, vars);
    if (!sym)
        return 0;
    if (stmt->var_decl.type == TYPE_ARRAY && stmt->var_decl.array_size == 0 &&
        stmt->var_decl.size_expr) {
        if (!handle_vla_size(stmt, sym, vars, funcs, ir))
            return 0;
    }
    return emit_var_initializer(stmt, sym, vars, funcs, ir);
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
    if (!label_format_suffix("L", id, "_else", else_label) ||
        !label_format_suffix("L", id, "_end", end_label))
        return 0;
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
 * Validate a struct or union return expression. The type must match
 * the function signature and the size of the returned aggregate must
 * agree with the expected size recorded for the function. Returns
 * non-zero on success.
 */
static int validate_struct_return(stmt_t *stmt, symtable_t *vars,
                                  symtable_t *funcs, type_kind_t expr_type,
                                  type_kind_t func_ret_type)
{
    if (expr_type != func_ret_type) {
        error_set(stmt->ret.expr->line, stmt->ret.expr->column,
                  error_current_file, error_current_function);
        return 0;
    }

    symbol_t *func_sym = symtable_lookup(funcs,
                                         error_current_function ?
                                         error_current_function : "");
    size_t expected = func_sym ? func_sym->ret_struct_size : 0;
    size_t actual = 0;

    if (stmt->ret.expr->kind == EXPR_IDENT) {
        symbol_t *vsym = symtable_lookup(vars, stmt->ret.expr->ident.name);
        if (vsym)
            actual = (expr_type == TYPE_STRUCT)
                ? vsym->struct_total_size : vsym->total_size;
    } else if (stmt->ret.expr->kind == EXPR_CALL) {
        symbol_t *fsym = symtable_lookup(funcs, stmt->ret.expr->call.name);
        if (!fsym)
            fsym = symtable_lookup(vars, stmt->ret.expr->call.name);
        if (fsym)
            actual = fsym->ret_struct_size;
    } else if (stmt->ret.expr->kind == EXPR_COMPLIT) {
        actual = stmt->ret.expr->compound.elem_size;
    }

    if (expected && actual && expected != actual) {
        error_set(stmt->ret.expr->line, stmt->ret.expr->column,
                  error_current_file, error_current_function);
        return 0;
    }

    return 1;
}

/*
 * Handle a return statement. The optional expression is validated
 * and emitted as the function result. For void functions a zero
 * constant is returned when no expression is present.
 */
static int handle_return_stmt(stmt_t *stmt, symtable_t *vars,
                              symtable_t *funcs, ir_builder_t *ir,
                              type_kind_t func_ret_type)
{
    if (!stmt->ret.expr) {
        if (func_ret_type != TYPE_VOID) {
            error_set(stmt->line, stmt->column, error_current_file, error_current_function);
            return 0;
        }
        ir_value_t zero = ir_build_const(ir, 0);
        ir_build_return(ir, zero);
        return 1;
    }

    ir_value_t val;
    type_kind_t vt = check_expr(stmt->ret.expr, vars, funcs, ir, &val);
    if (vt == TYPE_UNKNOWN) {
        error_set(stmt->ret.expr->line, stmt->ret.expr->column,
                  error_current_file, error_current_function);
        return 0;
    }

    if (func_ret_type == TYPE_STRUCT || func_ret_type == TYPE_UNION) {
        if (!validate_struct_return(stmt, vars, funcs, vt, func_ret_type))
            return 0;
        ir_value_t ret_ptr = ir_build_load_param(ir, 0);
        ir_build_store_ptr(ir, ret_ptr, val);
        ir_build_return_agg(ir, ret_ptr);
        return 1;
    }

    ir_build_return(ir, val);
    return 1;
}


/*
 * Handle a label statement. The label name is recorded in the table
 * and a corresponding IR label is emitted.
 */
static int handle_label_stmt(stmt_t *stmt, label_table_t *labels,
                             ir_builder_t *ir)
{
    const char *ir_name = label_table_get_or_add(labels, stmt->label.name);
    ir_build_label(ir, ir_name);
    return 1;
}

/*
 * Validate an expression statement by evaluating the expression and
 * discarding the result.
 */
static int check_expr_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                           ir_builder_t *ir)
{
    ir_value_t tmp;
    return check_expr(stmt->expr.expr, vars, funcs, ir, &tmp) != TYPE_UNKNOWN;
}

/*
 * Wrapper around the return statement handler used by check_stmt.
 */
static int check_return_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                             ir_builder_t *ir, type_kind_t func_ret_type)
{
    return handle_return_stmt(stmt, vars, funcs, ir, func_ret_type);
}

/*
 * Wrapper used to validate an if/else statement.
 */
static int check_if_stmt_wrapper(stmt_t *stmt, symtable_t *vars,
                                 symtable_t *funcs, label_table_t *labels,
                                 ir_builder_t *ir, type_kind_t func_ret_type,
                                 const char *break_label,
                                 const char *continue_label)
{
    return check_if_stmt(stmt, vars, funcs, labels, ir, func_ret_type,
                         break_label, continue_label);
}

/*
 * Wrapper used to validate a while loop.
 */
static int check_while_stmt_wrapper(stmt_t *stmt, symtable_t *vars,
                                    symtable_t *funcs, label_table_t *labels,
                                    ir_builder_t *ir,
                                    type_kind_t func_ret_type)
{
    return check_while_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
}

/*
 * Wrapper used to validate a do-while loop.
 */
static int check_do_while_stmt_wrapper(stmt_t *stmt, symtable_t *vars,
                                       symtable_t *funcs,
                                       label_table_t *labels,
                                       ir_builder_t *ir,
                                       type_kind_t func_ret_type)
{
    return check_do_while_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
}

/*
 * Wrapper used to validate a for loop.
 */
static int check_for_stmt_wrapper(stmt_t *stmt, symtable_t *vars,
                                  symtable_t *funcs, label_table_t *labels,
                                  ir_builder_t *ir,
                                  type_kind_t func_ret_type)
{
    return check_for_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
}

/*
 * Wrapper used to validate a switch statement.
 */
static int check_switch_stmt_wrapper(stmt_t *stmt, symtable_t *vars,
                                     symtable_t *funcs,
                                     label_table_t *labels,
                                     ir_builder_t *ir,
                                     type_kind_t func_ret_type)
{
    return check_switch_stmt(stmt, vars, funcs, labels, ir, func_ret_type);
}

/*
 * Validate a goto statement by resolving the target label and
 * emitting a branch.
 */
static int check_goto_stmt(stmt_t *stmt, label_table_t *labels,
                           ir_builder_t *ir)
{
    const char *ir_name = label_table_get_or_add(labels, stmt->goto_stmt.name);
    ir_build_br(ir, ir_name);
    return 1;
}


/* Evaluate a _Static_assert expression and emit an error if zero */
static int check_static_assert_stmt(stmt_t *stmt, symtable_t *vars)
{
    long long val;
    if (!eval_const_expr(stmt->static_assert.expr, vars, 0, &val)) {
        error_set(stmt->static_assert.expr->line, stmt->static_assert.expr->column,
                  error_current_file, error_current_function);
        return 0;
    }
    if (val == 0) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        error_print(stmt->static_assert.message);
        return 0;
    }
    return 1;
}

/*
 * Validate a compound statement by recursively checking each contained
 * statement and restoring the variable scope on exit.
 */
static int check_block_stmt_wrapper(stmt_t *stmt, symtable_t *vars,
                                    symtable_t *funcs, label_table_t *labels,
                                    ir_builder_t *ir,
                                    type_kind_t func_ret_type,
                                    const char *break_label,
                                    const char *continue_label)
{
    symbol_t *old_head = vars->head;
    for (size_t i = 0; i < stmt->block.count; i++) {
        if (!check_stmt(stmt->block.stmts[i], vars, funcs, labels, ir,
                        func_ret_type, break_label, continue_label)) {
            symtable_pop_scope(vars, old_head);
            return 0;
        }
    }
    symtable_pop_scope(vars, old_head);
    return 1;
}

/*
 * Wrapper for enum declarations.
 */
static int check_enum_decl_stmt_wrapper(stmt_t *stmt, symtable_t *vars)
{
    return check_enum_decl_stmt(stmt, vars);
}

/*
 * Wrapper for struct declarations.
 */
static int check_struct_decl_stmt_wrapper(stmt_t *stmt, symtable_t *vars)
{
    return check_struct_decl_stmt(stmt, vars);
}

/*
 * Wrapper for union declarations.
 */
static int check_union_decl_stmt_wrapper(stmt_t *stmt, symtable_t *vars)
{
    return check_union_decl_stmt(stmt, vars);
}

/*
 * Wrapper for typedef declarations.
 */
static int check_typedef_stmt_wrapper(stmt_t *stmt, symtable_t *vars)
{
    return check_typedef_stmt(stmt, vars);
}

/*
 * Wrapper for variable declarations.
 */
static int check_var_decl_stmt_wrapper(stmt_t *stmt, symtable_t *vars,
                                       symtable_t *funcs, ir_builder_t *ir)
{
    return check_var_decl_stmt(stmt, vars, funcs, ir);
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
    ir_builder_set_loc(ir, error_current_file, stmt->line, stmt->column);
    switch (stmt->kind) {
    case STMT_EXPR:
        return check_expr_stmt(stmt, vars, funcs, ir);
    case STMT_RETURN:
        return check_return_stmt(stmt, vars, funcs, ir, func_ret_type);
    case STMT_IF:
        return check_if_stmt_wrapper(stmt, vars, funcs, labels, ir, func_ret_type,
                                     break_label, continue_label);
    case STMT_WHILE:
        return check_while_stmt_wrapper(stmt, vars, funcs, labels, ir,
                                        func_ret_type);
    case STMT_DO_WHILE:
        return check_do_while_stmt_wrapper(stmt, vars, funcs, labels, ir,
                                           func_ret_type);
    case STMT_FOR:
        return check_for_stmt_wrapper(stmt, vars, funcs, labels, ir, func_ret_type);
    case STMT_SWITCH:
        return check_switch_stmt_wrapper(stmt, vars, funcs, labels, ir,
                                         func_ret_type);
    case STMT_LABEL:
        return handle_label_stmt(stmt, labels, ir);
    case STMT_GOTO:
        return check_goto_stmt(stmt, labels, ir);
    case STMT_STATIC_ASSERT:
        return check_static_assert_stmt(stmt, vars);
    case STMT_BREAK:
        return check_break_stmt(stmt, break_label, ir);
    case STMT_CONTINUE:
        return check_continue_stmt(stmt, continue_label, ir);
    case STMT_BLOCK:
        return check_block_stmt_wrapper(stmt, vars, funcs, labels, ir,
                                        func_ret_type, break_label,
                                        continue_label);
    case STMT_ENUM_DECL:
        return check_enum_decl_stmt_wrapper(stmt, vars);
    case STMT_STRUCT_DECL:
        return check_struct_decl_stmt_wrapper(stmt, vars);
    case STMT_UNION_DECL:
        return check_union_decl_stmt_wrapper(stmt, vars);
    case STMT_TYPEDEF:
        return check_typedef_stmt_wrapper(stmt, vars);
    case STMT_VAR_DECL:
        return check_var_decl_stmt_wrapper(stmt, vars, funcs, ir);
    }
    return 0;
}

/* Issue warnings for unreachable statements after return or goto end */
static const char *find_end_label(func_t *func)
{
    for (size_t i = func->body_count; i-- > 0;) {
        stmt_t *s = func->body[i];
        if (s->kind == STMT_LABEL)
            return s->label.name;
        if (s->kind != STMT_RETURN)
            break;
    }
    return NULL;
}

void warn_unreachable_function(func_t *func, symtable_t *funcs)
{
    if (!semantic_warn_unreachable || !func)
        return;

    const char *end_label = find_end_label(func);
    int reachable = 1;
    for (size_t i = 0; i < func->body_count; i++) {
        stmt_t *s = func->body[i];
        if (!reachable) {
            if (!(s->kind == STMT_LABEL && end_label &&
                  strcmp(s->label.name, end_label) == 0)) {
                error_set(s->line, s->column, error_current_file,
                          error_current_function);
                error_print("warning: unreachable statement");
            }
        }
        if (s->kind == STMT_RETURN)
            reachable = 0;
        else if (s->kind == STMT_GOTO && end_label &&
                 strcmp(s->goto_stmt.name, end_label) == 0)
            reachable = 0;
        else if (s->kind == STMT_LABEL && end_label &&
                 strcmp(s->label.name, end_label) == 0)
            reachable = 1;
        else if (s->kind == STMT_EXPR && s->expr.expr &&
                 s->expr.expr->kind == EXPR_CALL) {
            symbol_t *fs = symtable_lookup(funcs, s->expr.expr->call.name);
            if (fs && fs->is_noreturn) {
                if (i + 1 < func->body_count &&
                    !(func->body[i+1]->kind == STMT_LABEL && end_label &&
                      strcmp(func->body[i+1]->label.name, end_label) == 0)) {
                    error_set(s->line, s->column, error_current_file,
                              error_current_function);
                    error_print("warning: non-returning call without terminator");
                }
                reachable = 0;
            }
        }
    }
}
