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
 * Compute layout information for an aggregate variable.  Struct and
 * union members are assigned offsets and the resulting element size is
 * stored back into the declaration.
 */
static void compute_var_layout(stmt_t *stmt)
{
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
}

/*
 * Insert the variable into the given symbol table using the provided
 * IR name.  If successful the inserted symbol is returned, otherwise
 * NULL is returned.
 */
static symbol_t *insert_var_symbol(stmt_t *stmt, symtable_t *vars,
                                   const char *ir_name)
{
    if (!symtable_add(vars, stmt->var_decl.name, ir_name,
                      stmt->var_decl.type,
                      stmt->var_decl.array_size,
                      stmt->var_decl.elem_size,
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
    if (stmt->var_decl.is_static)
        ir_name = label_format("__static", label_next_id(), ir_name_buf);

    compute_var_layout(stmt);
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
static int copy_union_metadata(symbol_t *sym, union_member_t *members,
                               size_t count, size_t total)
{
    sym->total_size = total;
    if (!count)
        return 1;
    sym->members = malloc(count * sizeof(*sym->members));
    if (!sym->members)
        return 0;
    sym->member_count = count;
    for (size_t i = 0; i < count; i++) {
        union_member_t *m = &members[i];
        sym->members[i].name = vc_strdup(m->name);
        sym->members[i].type = m->type;
        sym->members[i].elem_size = m->elem_size;
        sym->members[i].offset = m->offset;
        sym->members[i].bit_width = m->bit_width;
        sym->members[i].bit_offset = m->bit_offset;
    }
    return 1;
}

/*
 * Copy struct member metadata from the parsed declaration to the symbol.
 * Allocates new member arrays and duplicates names. Returns non-zero on
 * success.
 */
static int copy_struct_metadata(symbol_t *sym, struct_member_t *members,
                                size_t count, size_t total)
{
    sym->struct_total_size = total;
    if (!count)
        return 1;
    sym->struct_members = malloc(count * sizeof(*sym->struct_members));
    if (!sym->struct_members)
        return 0;
    sym->struct_member_count = count;
    for (size_t i = 0; i < count; i++) {
        struct_member_t *m = &members[i];
        sym->struct_members[i].name = vc_strdup(m->name);
        sym->struct_members[i].type = m->type;
        sym->struct_members[i].elem_size = m->elem_size;
        sym->struct_members[i].offset = m->offset;
        sym->struct_members[i].bit_width = m->bit_width;
        sym->struct_members[i].bit_offset = m->bit_offset;
    }
    return 1;
}

/*
 * Emit IR for a static initializer using a constant expression.
 * Handles scalars and aggregates placed in the global data section.
 */
static int emit_static_initializer(stmt_t *stmt, symbol_t *sym,
                                   symtable_t *vars, ir_builder_t *ir)
{
    long long cval;
    if (!eval_const_expr(stmt->var_decl.init, vars, &cval)) {
        error_set(stmt->var_decl.init->line, stmt->var_decl.init->column, error_current_file, error_current_function);
        return 0;
    }
    if (stmt->var_decl.type == TYPE_UNION)
        ir_build_glob_union(ir, sym->ir_name, (int)sym->elem_size, 1);
    else if (stmt->var_decl.type == TYPE_STRUCT)
        ir_build_glob_struct(ir, sym->ir_name, (int)sym->struct_total_size, 1);
    else
        ir_build_glob_var(ir, sym->ir_name, cval, 1);
    return 1;
}

/*
 * Emit IR for a dynamic initializer evaluated at runtime.
 * The expression is checked for type compatibility and then stored.
 */
static int emit_dynamic_initializer(stmt_t *stmt, symbol_t *sym,
                                    symtable_t *vars, symtable_t *funcs,
                                    ir_builder_t *ir)
{
    ir_value_t val;
    type_kind_t vt = check_expr(stmt->var_decl.init, vars, funcs, ir, &val);
    if (!(((is_intlike(stmt->var_decl.type) && is_intlike(vt)) ||
           (is_floatlike(stmt->var_decl.type) &&
            (is_floatlike(vt) || is_intlike(vt)))) ||
          vt == stmt->var_decl.type)) {
        error_set(stmt->var_decl.init->line, stmt->var_decl.init->column, error_current_file, error_current_function);
        return 0;
    }
    if (stmt->var_decl.is_volatile)
        ir_build_store_vol(ir, sym->ir_name, val);
    else
        ir_build_store(ir, sym->ir_name, val);
    return 1;
}

/*
 * Emit IR for an initializer list of an aggregate variable.
 * Dispatches to the array or struct handler as appropriate.
 */
static int emit_aggregate_initializer(stmt_t *stmt, symbol_t *sym,
                                      symtable_t *vars, ir_builder_t *ir)
{
    if (stmt->var_decl.type == TYPE_ARRAY)
        return handle_array_init(stmt, sym, vars, ir);
    if (stmt->var_decl.type == TYPE_STRUCT)
        return handle_struct_init(stmt, sym, vars, ir);
    error_set(stmt->line, stmt->column, error_current_file, error_current_function);
    return 0;
}

/*
 * Copy metadata describing aggregate members from the declaration to the
 * symbol table entry.  Dispatches to the struct or union helper based on the
 * variable's type.  Returns non-zero on success.
 */
static int copy_aggregate_metadata(stmt_t *stmt, symbol_t *sym)
{
    if (stmt->var_decl.type == TYPE_UNION)
        return copy_union_metadata(sym, stmt->var_decl.members,
                                   stmt->var_decl.member_count,
                                   stmt->var_decl.elem_size);

    if (stmt->var_decl.type == TYPE_STRUCT)
        return copy_struct_metadata(sym,
                                    (struct_member_t *)stmt->var_decl.members,
                                    stmt->var_decl.member_count,
                                    stmt->var_decl.elem_size);

    return 1;
}

/*
 * Emit IR for a static initializer.  This simply wraps the existing helper
 * used to handle constant expression initializers for scalars and aggregates.
 */
static int emit_static_init(stmt_t *stmt, symbol_t *sym, symtable_t *vars,
                            ir_builder_t *ir)
{
    return emit_static_initializer(stmt, sym, vars, ir);
}

/*
 * Emit IR for a dynamic initializer evaluated at run time.  The expression is
 * validated and then stored to the variable.
 */
static int emit_dynamic_init(stmt_t *stmt, symbol_t *sym, symtable_t *vars,
                             symtable_t *funcs, ir_builder_t *ir)
{
    return emit_dynamic_initializer(stmt, sym, vars, funcs, ir);
}

/*
 * Emit IR for an initializer list of an aggregate type.  This delegates to the
 * array or struct handlers via the existing aggregate initializer helper.
 */
static int emit_aggregate_init(stmt_t *stmt, symbol_t *sym,
                               symtable_t *vars, ir_builder_t *ir)
{
    return emit_aggregate_initializer(stmt, sym, vars, ir);
}

/*
 * Emit IR for any initializer attached to the variable.
 * Copies aggregate member metadata to the symbol and writes
 * constant or computed values using the IR builder.
 * Returns non-zero on success.
 */
static int emit_var_initializer(stmt_t *stmt, symbol_t *sym,
                                symtable_t *vars, symtable_t *funcs,
                                ir_builder_t *ir)
{
    if (!copy_aggregate_metadata(stmt, sym))
        return 0;

    if (stmt->var_decl.init)
        return stmt->var_decl.is_static
                   ? emit_static_init(stmt, sym, vars, ir)
                   : emit_dynamic_init(stmt, sym, vars, funcs, ir);

    if (stmt->var_decl.init_list)
        return emit_aggregate_init(stmt, sym, vars, ir);

    return 1;
}

static int check_var_decl_stmt(stmt_t *stmt, symtable_t *vars,
                               symtable_t *funcs, ir_builder_t *ir)
{
    symbol_t *sym = register_var_symbol(stmt, vars);
    if (!sym)
        return 0;
    if (stmt->var_decl.type == TYPE_ARRAY && stmt->var_decl.array_size == 0 &&
        stmt->var_decl.size_expr) {
        ir_value_t lenv;
        if (check_expr(stmt->var_decl.size_expr, vars, funcs, ir, &lenv) ==
            TYPE_UNKNOWN)
            return 0;
        ir_value_t eszv = ir_build_const(ir, (int)stmt->var_decl.elem_size);
        ir_value_t total = ir_build_binop(ir, IR_MUL, lenv, eszv);
        sym->vla_addr = ir_build_alloca(ir, total);
        sym->vla_size = lenv;
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
    if (check_expr(stmt->ret.expr, vars, funcs, ir, &val) == TYPE_UNKNOWN) {
        error_set(stmt->ret.expr->line, stmt->ret.expr->column, error_current_file, error_current_function);
        return 0;
    }
    ir_build_return(ir, val);
    return 1;
}

/*
 * Handle break and continue statements. The target label must be
 * provided by the caller. If no valid label is available an error
 * is reported.
 */
static int handle_loop_stmt(stmt_t *stmt, const char *target,
                            ir_builder_t *ir)
{
    if (!target) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        return 0;
    }
    ir_build_br(ir, target);
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

/*
 * Wrapper used to validate a break statement.
 */
static int check_break_stmt(stmt_t *stmt, const char *break_label,
                            ir_builder_t *ir)
{
    return handle_loop_stmt(stmt, break_label, ir);
}

/*
 * Wrapper used to validate a continue statement.
 */
static int check_continue_stmt(stmt_t *stmt, const char *continue_label,
                               ir_builder_t *ir)
{
    return handle_loop_stmt(stmt, continue_label, ir);
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
