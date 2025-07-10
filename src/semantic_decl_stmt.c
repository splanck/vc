#include <stdlib.h>
#include <string.h>
#include "semantic_decl_stmt.h"
#include "semantic_var.h"
#include "semantic_layout.h"
#include "consteval.h"
#include "semantic_control.h"
#include "ir_core.h"
#include "label.h"
#include "error.h"

static int check_enum_decl_stmt(stmt_t *stmt, symtable_t *vars)
{
    int next = 0;
    for (size_t i = 0; i < stmt->enum_decl.count; i++) {
        enumerator_t *e = &stmt->enum_decl.items[i];
        long long val = next;
        if (e->value) {
            if (!eval_const_expr(e->value, vars, 0, &val)) {
                error_set(e->value->line, e->value->column, error_current_file,
                          error_current_function);
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

int stmt_enum_decl_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                           label_table_t *labels, ir_builder_t *ir,
                           type_kind_t func_ret_type,
                           const char *break_label,
                           const char *continue_label)
{
    (void)funcs; (void)labels; (void)ir; (void)func_ret_type;
    (void)break_label; (void)continue_label;
    return check_enum_decl_stmt(stmt, vars);
}

int stmt_struct_decl_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                             label_table_t *labels, ir_builder_t *ir,
                             type_kind_t func_ret_type,
                             const char *break_label,
                             const char *continue_label)
{
    (void)funcs; (void)labels; (void)ir; (void)func_ret_type;
    (void)break_label; (void)continue_label;
    return check_struct_decl_stmt(stmt, vars);
}

int stmt_union_decl_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                            label_table_t *labels, ir_builder_t *ir,
                            type_kind_t func_ret_type,
                            const char *break_label,
                            const char *continue_label)
{
    (void)funcs; (void)labels; (void)ir; (void)func_ret_type;
    (void)break_label; (void)continue_label;
    return check_union_decl_stmt(stmt, vars);
}

int stmt_var_decl_handler(stmt_t *stmt, symtable_t *vars,
                          symtable_t *funcs, label_table_t *labels,
                          ir_builder_t *ir, type_kind_t func_ret_type,
                          const char *break_label,
                          const char *continue_label)
{
    (void)labels; (void)func_ret_type; (void)break_label; (void)continue_label;
    return check_var_decl_stmt(stmt, vars, funcs, ir);
}

