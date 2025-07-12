#include "semantic_stmt.h"
#include "semantic_expr.h"
#include "symtable.h"
#include "semantic_control.h"
#include "ir_core.h"
#include "error.h"

static int validate_struct_return(stmt_t *stmt, symtable_t *vars,
                                  symtable_t *funcs, type_kind_t expr_type,
                                  type_kind_t func_ret_type)
{
    if (expr_type != func_ret_type) {
        error_set(STMT_RET(stmt).expr->line, STMT_RET(stmt).expr->column,
                  error_current_file, error_current_function);
        return 0;
    }

    symbol_t *func_sym = symtable_lookup(funcs,
                                         error_current_function ?
                                         error_current_function : "");
    size_t expected = func_sym ? func_sym->ret_struct_size : 0;
    size_t actual = 0;

    if (STMT_RET(stmt).expr->kind == EXPR_IDENT) {
        symbol_t *vsym = symtable_lookup(vars, STMT_RET(stmt).expr->data.ident.name);
        if (vsym)
            actual = (expr_type == TYPE_STRUCT)
                ? vsym->struct_total_size : vsym->total_size;
    } else if (STMT_RET(stmt).expr->kind == EXPR_CALL) {
        symbol_t *fsym = symtable_lookup(funcs, STMT_RET(stmt).expr->data.call.name);
        if (!fsym)
            fsym = symtable_lookup(vars, STMT_RET(stmt).expr->data.call.name);
        if (fsym)
            actual = fsym->ret_struct_size;
    } else if (STMT_RET(stmt).expr->kind == EXPR_COMPLIT) {
        actual = STMT_RET(stmt).expr->data.compound.elem_size;
    }

    if (expected && actual && expected != actual) {
        error_set(STMT_RET(stmt).expr->line, STMT_RET(stmt).expr->column,
                  error_current_file, error_current_function);
        return 0;
    }

    return 1;
}

static int handle_return_stmt(stmt_t *stmt, symtable_t *vars,
                              symtable_t *funcs, ir_builder_t *ir,
                              type_kind_t func_ret_type)
{
    if (!STMT_RET(stmt).expr) {
        if (func_ret_type != TYPE_VOID) {
            error_set(stmt->line, stmt->column, error_current_file, error_current_function);
            return 0;
        }
        ir_value_t zero = ir_build_const(ir, 0);
        ir_build_return(ir, zero);
        return 1;
    }

    ir_value_t val;
    type_kind_t vt = check_expr(STMT_RET(stmt).expr, vars, funcs, ir, &val);
    if (vt == TYPE_UNKNOWN) {
        error_set(STMT_RET(stmt).expr->line, STMT_RET(stmt).expr->column,
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

int stmt_return_handler(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
                        label_table_t *labels, ir_builder_t *ir,
                        type_kind_t func_ret_type,
                        const char *break_label,
                        const char *continue_label)
{
    (void)labels; (void)break_label; (void)continue_label;
    return handle_return_stmt(stmt, vars, funcs, ir, func_ret_type);
}

