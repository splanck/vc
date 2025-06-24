#include <stdlib.h>
#include <stdio.h>
#include "semantic.h"
#include "label.h"

static int is_intlike(type_kind_t t)
{
    return t == TYPE_INT || t == TYPE_CHAR;
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

int check_stmt(stmt_t *stmt, symtable_t *vars, symtable_t *funcs,
               ir_builder_t *ir, type_kind_t func_ret_type,
               const char *break_label, const char *continue_label)
{
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
                semantic_set_error(stmt->line, stmt->column);
                return 0;
            }
            ir_value_t zero = ir_build_const(ir, 0);
            ir_build_return(ir, zero);
            return 1;
        }
        ir_value_t val;
        if (check_expr(stmt->ret.expr, vars, funcs, ir, &val) == TYPE_UNKNOWN) {
            semantic_set_error(stmt->ret.expr->line, stmt->ret.expr->column);
            return 0;
        }
        ir_build_return(ir, val);
        return 1;
    }
    case STMT_IF: {
        ir_value_t cond_val;
        if (check_expr(stmt->if_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
            return 0;
        char else_label[32];
        char end_label[32];
        int id = label_next_id();
        snprintf(else_label, sizeof(else_label), "L%d_else", id);
        snprintf(end_label, sizeof(end_label), "L%d_end", id);
        const char *target = stmt->if_stmt.else_branch ? else_label : end_label;
        ir_build_bcond(ir, cond_val, target);
        if (!check_stmt(stmt->if_stmt.then_branch, vars, funcs, ir, func_ret_type,
                        break_label, continue_label))
            return 0;
        if (stmt->if_stmt.else_branch) {
            ir_build_br(ir, end_label);
            ir_build_label(ir, else_label);
            if (!check_stmt(stmt->if_stmt.else_branch, vars, funcs, ir, func_ret_type,
                            break_label, continue_label))
                return 0;
        }
        ir_build_label(ir, end_label);
        return 1;
    }
    case STMT_WHILE: {
        ir_value_t cond_val;
        char start_label[32];
        char end_label[32];
        int id = label_next_id();
        snprintf(start_label, sizeof(start_label), "L%d_start", id);
        snprintf(end_label, sizeof(end_label), "L%d_end", id);
        ir_build_label(ir, start_label);
        if (check_expr(stmt->while_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
            return 0;
        ir_build_bcond(ir, cond_val, end_label);
        if (!check_stmt(stmt->while_stmt.body, vars, funcs, ir, func_ret_type,
                        end_label, start_label))
            return 0;
        ir_build_br(ir, start_label);
        ir_build_label(ir, end_label);
        return 1;
    }
    case STMT_DO_WHILE: {
        ir_value_t cond_val;
        char start_label[32];
        char cond_label[32];
        char end_label[32];
        int id = label_next_id();
        snprintf(start_label, sizeof(start_label), "L%d_start", id);
        snprintf(cond_label, sizeof(cond_label), "L%d_cond", id);
        snprintf(end_label, sizeof(end_label), "L%d_end", id);
        ir_build_label(ir, start_label);
        if (!check_stmt(stmt->do_while_stmt.body, vars, funcs, ir, func_ret_type,
                        end_label, cond_label))
            return 0;
        ir_build_label(ir, cond_label);
        if (check_expr(stmt->do_while_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
            return 0;
        ir_build_bcond(ir, cond_val, end_label);
        ir_build_br(ir, start_label);
        ir_build_label(ir, end_label);
        return 1;
    }
    case STMT_FOR: {
        ir_value_t cond_val;
        char start_label[32];
        char end_label[32];
        int id = label_next_id();
        snprintf(start_label, sizeof(start_label), "L%d_start", id);
        snprintf(end_label, sizeof(end_label), "L%d_end", id);
        if (check_expr(stmt->for_stmt.init, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
            return 0; /* reuse cond_val for init but ignore value */
        ir_build_label(ir, start_label);
        if (check_expr(stmt->for_stmt.cond, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
            return 0;
        ir_build_bcond(ir, cond_val, end_label);
        char cont_label[32];
        snprintf(cont_label, sizeof(cont_label), "L%d_cont", id);
        if (!check_stmt(stmt->for_stmt.body, vars, funcs, ir, func_ret_type,
                        end_label, cont_label))
            return 0;
        ir_build_label(ir, cont_label);
        if (check_expr(stmt->for_stmt.incr, vars, funcs, ir, &cond_val) == TYPE_UNKNOWN)
            return 0;
        ir_build_br(ir, start_label);
        ir_build_label(ir, end_label);
        return 1;
    }
    case STMT_BREAK:
        if (!break_label) {
            semantic_set_error(stmt->line, stmt->column);
            return 0;
        }
        ir_build_br(ir, break_label);
        return 1;
    case STMT_CONTINUE:
        if (!continue_label) {
            semantic_set_error(stmt->line, stmt->column);
            return 0;
        }
        ir_build_br(ir, continue_label);
        return 1;
    case STMT_BLOCK: {
        symbol_t *old_head = vars->head;
        for (size_t i = 0; i < stmt->block.count; i++) {
            if (!check_stmt(stmt->block.stmts[i], vars, funcs, ir, func_ret_type,
                            break_label, continue_label)) {
                symtable_pop_scope(vars, old_head);
                return 0;
            }
        }
        symtable_pop_scope(vars, old_head);
        return 1;
    }
    case STMT_VAR_DECL: {
        if (!symtable_add(vars, stmt->var_decl.name, stmt->var_decl.type,
                          stmt->var_decl.array_size)) {
            semantic_set_error(stmt->line, stmt->column);
            return 0;
        }
        if (stmt->var_decl.init) {
            ir_value_t val;
            type_kind_t vt = check_expr(stmt->var_decl.init, vars, funcs, ir, &val);
            if (!((stmt->var_decl.type == TYPE_CHAR && is_intlike(vt)) ||
                  vt == stmt->var_decl.type)) {
                semantic_set_error(stmt->var_decl.init->line, stmt->var_decl.init->column);
                return 0;
            }
            ir_build_store(ir, stmt->var_decl.name, val);
        } else if (stmt->var_decl.init_list) {
            if (stmt->var_decl.type != TYPE_ARRAY ||
                stmt->var_decl.array_size < stmt->var_decl.init_count) {
                semantic_set_error(stmt->line, stmt->column);
                return 0;
            }
            for (size_t i = 0; i < stmt->var_decl.init_count; i++) {
                int v;
                if (!eval_const_expr(stmt->var_decl.init_list[i], &v)) {
                    semantic_set_error(stmt->var_decl.init_list[i]->line,
                                      stmt->var_decl.init_list[i]->column);
                    return 0;
                }
                ir_value_t idx = ir_build_const(ir, (int)i);
                ir_value_t val = ir_build_const(ir, v);
                ir_build_store_idx(ir, stmt->var_decl.name, idx, val);
            }
        }
        return 1;
    }
    }
    return 0;
}
