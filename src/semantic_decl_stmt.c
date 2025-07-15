#include <stdlib.h>
#include <string.h>
#include "semantic_decl_stmt.h"
#include "semantic_var.h"
#include "semantic_layout.h"
#include "consteval.h"
#include "semantic_control.h"
#include "semantic_global.h"
#include <stdio.h>
#include "ir_core.h"
#include "label.h"
#include "error.h"

/* Compute the size in bytes of a symbol for stack allocation */
static size_t local_sym_size(symbol_t *sym)
{
    switch (sym->type) {
    case TYPE_CHAR: case TYPE_UCHAR: case TYPE_BOOL:
        return 1;
    case TYPE_SHORT: case TYPE_USHORT:
        return 2;
    case TYPE_INT: case TYPE_UINT: case TYPE_LONG: case TYPE_ULONG:
    case TYPE_ENUM: case TYPE_PTR:
        return 4;
    case TYPE_LLONG: case TYPE_ULLONG:
        return 8;
    case TYPE_ARRAY:
        return sym->array_size * sym->elem_size;
    case TYPE_STRUCT:
        return sym->struct_total_size;
    case TYPE_UNION:
        return sym->total_size;
    default:
        return 0;
    }
}

static int check_enum_decl_stmt(stmt_t *stmt, symtable_t *vars)
{
    int next = 0;
    for (size_t i = 0; i < STMT_ENUM_DECL(stmt).count; i++) {
        enumerator_t *e = &STMT_ENUM_DECL(stmt).items[i];
        long long val = next;
        if (e->value) {
            if (!eval_const_expr(e->value, vars,
                                 semantic_get_x86_64(), &val)) {
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
    if (STMT_ENUM_DECL(stmt).tag && STMT_ENUM_DECL(stmt).tag[0])
        symtable_add_enum_tag(vars, STMT_ENUM_DECL(stmt).tag);
    return 1;
}

static int check_struct_decl_stmt(stmt_t *stmt, symtable_t *vars)
{
    size_t total = layout_struct_members(STMT_STRUCT_DECL(stmt).members,
                                         STMT_STRUCT_DECL(stmt).count);
    if (!symtable_add_struct(vars, STMT_STRUCT_DECL(stmt).tag,
                             STMT_STRUCT_DECL(stmt).members,
                             STMT_STRUCT_DECL(stmt).count)) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        return 0;
    }
    symbol_t *stype = symtable_lookup_struct(vars, STMT_STRUCT_DECL(stmt).tag);
    if (stype)
        stype->struct_total_size = total;
    return 1;
}

static int check_union_decl_stmt(stmt_t *stmt, symtable_t *vars)
{
    size_t max = layout_union_members(STMT_UNION_DECL(stmt).members,
                                      STMT_UNION_DECL(stmt).count);
    (void)max;
    if (!symtable_add_union(vars, STMT_UNION_DECL(stmt).tag,
                            STMT_UNION_DECL(stmt).members,
                            STMT_UNION_DECL(stmt).count)) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        return 0;
    }
    return 1;
}

static symbol_t *insert_var_symbol(stmt_t *stmt, symtable_t *vars,
                                   const char *ir_name)
{
    if (STMT_VAR_DECL(stmt).align_expr) {
        long long aval;
        if (!eval_const_expr(STMT_VAR_DECL(stmt).align_expr, vars,
                             semantic_get_x86_64(), &aval) ||
            aval <= 0 || (aval & (aval - 1))) {
            error_set(STMT_VAR_DECL(stmt).align_expr->line,
                      STMT_VAR_DECL(stmt).align_expr->column,
                      error_current_file, error_current_function);
            error_print("Invalid alignment");
            return NULL;
        }
        STMT_VAR_DECL(stmt).alignment = (size_t)aval;
    }
    if (!symtable_add(vars, STMT_VAR_DECL(stmt).name, ir_name,
                      STMT_VAR_DECL(stmt).type,
                      STMT_VAR_DECL(stmt).array_size,
                      STMT_VAR_DECL(stmt).elem_size,
                      STMT_VAR_DECL(stmt).alignment,
                      STMT_VAR_DECL(stmt).is_static,
                      STMT_VAR_DECL(stmt).is_register,
                      STMT_VAR_DECL(stmt).is_const,
                      STMT_VAR_DECL(stmt).is_volatile,
                      STMT_VAR_DECL(stmt).is_restrict))
        return NULL;

    symbol_t *sym = symtable_lookup(vars, STMT_VAR_DECL(stmt).name);
    if (STMT_VAR_DECL(stmt).init_list && STMT_VAR_DECL(stmt).type == TYPE_ARRAY &&
        STMT_VAR_DECL(stmt).array_size == 0) {
        STMT_VAR_DECL(stmt).array_size = STMT_VAR_DECL(stmt).init_count;
        sym->array_size = STMT_VAR_DECL(stmt).array_size;
    }

    return sym;
}

static symbol_t *register_var_symbol(stmt_t *stmt, symtable_t *vars)
{
    char ir_name_buf[32];
    const char *ir_name = STMT_VAR_DECL(stmt).name;
    if (STMT_VAR_DECL(stmt).is_static) {
        if (!label_format("__static", label_next_id(), ir_name_buf))
            return NULL;
        ir_name = ir_name_buf;
    }

    if (!compute_var_layout(stmt, vars))
        return NULL;
    if (STMT_VAR_DECL(stmt).is_const && !STMT_VAR_DECL(stmt).init &&
        !STMT_VAR_DECL(stmt).init_list) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        return NULL;
    }
    symbol_t *sym = insert_var_symbol(stmt, vars, ir_name);
    if (!sym) {
        error_set(stmt->line, stmt->column, error_current_file, error_current_function);
        return NULL;
    }

    sym->func_ret_type = STMT_VAR_DECL(stmt).func_ret_type;
    sym->func_param_count = STMT_VAR_DECL(stmt).func_param_count;
    sym->func_variadic = STMT_VAR_DECL(stmt).func_variadic;
    if (STMT_VAR_DECL(stmt).func_param_count) {
        sym->func_param_types = malloc(sym->func_param_count * sizeof(type_kind_t));
        if (!sym->func_param_types)
            return NULL;
        for (size_t i = 0; i < sym->func_param_count; i++)
            sym->func_param_types[i] = STMT_VAR_DECL(stmt).func_param_types[i];
    }

    if (!STMT_VAR_DECL(stmt).is_static && !STMT_VAR_DECL(stmt).is_extern) {
        size_t sz = local_sym_size(sym);
        sz = (sz + 3) & ~3u;
        semantic_stack_offset += (int)sz;
        sym->stack_offset = semantic_stack_offset;
        char sbuf[32];
        snprintf(sbuf, sizeof(sbuf), "stack:%d", sym->stack_offset);
        free(sym->ir_name);
        sym->ir_name = vc_strdup(sbuf);
    }

    return sym;
}

static int check_var_decl_stmt(stmt_t *stmt, symtable_t *vars,
                               symtable_t *funcs, ir_builder_t *ir)
{
    symbol_t *sym = register_var_symbol(stmt, vars);
    if (!sym)
        return 0;
    if (STMT_VAR_DECL(stmt).type == TYPE_ARRAY && STMT_VAR_DECL(stmt).array_size == 0 &&
        STMT_VAR_DECL(stmt).size_expr) {
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

