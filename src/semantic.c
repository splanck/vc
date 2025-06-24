#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic.h"

static char *dup_string(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static int next_label_id = 0;

static size_t error_line = 0;
static size_t error_column = 0;

static void set_error(size_t line, size_t column)
{
    error_line = line;
    error_column = column;
}

void semantic_print_error(const char *msg)
{
    fprintf(stderr, "%s at line %zu, column %zu\n",
            msg, error_line, error_column);
}

void symtable_init(symtable_t *table)
{
    table->head = NULL;
    table->globals = NULL;
}

void symtable_free(symtable_t *table)
{
    symbol_t *sym = table->head;
    while (sym) {
        symbol_t *next = sym->next;
        free(sym->name);
        free(sym->param_types);
        free(sym);
        sym = next;
    }
    sym = table->globals;
    while (sym) {
        symbol_t *next = sym->next;
        free(sym->name);
        free(sym->param_types);
        free(sym);
        sym = next;
    }
    table->head = NULL;
    table->globals = NULL;
}

symbol_t *symtable_lookup(symtable_t *table, const char *name)
{
    for (symbol_t *sym = table->head; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return sym;
    }
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return sym;
    }
    return NULL;
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

int symtable_add(symtable_t *table, const char *name, type_kind_t type,
                 size_t array_size)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = malloc(sizeof(*sym));
    if (!sym)
        return 0;
    sym->name = dup_string(name ? name : "");
    if (!sym->name) {
        free(sym);
        return 0;
    }
    sym->type = type;
    sym->array_size = array_size;
    sym->param_index = -1;
    sym->param_types = NULL;
    sym->param_count = 0;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

int symtable_add_param(symtable_t *table, const char *name, type_kind_t type,
                       int index)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = malloc(sizeof(*sym));
    if (!sym)
        return 0;
    sym->name = dup_string(name ? name : "");
    if (!sym->name) {
        free(sym);
        return 0;
    }
    sym->type = type;
    sym->array_size = 0;
    sym->param_index = index;
    sym->param_types = NULL;
    sym->param_count = 0;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

int symtable_add_global(symtable_t *table, const char *name, type_kind_t type,
                        size_t array_size)
{
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return 0;
    }
    symbol_t *sym = malloc(sizeof(*sym));
    if (!sym)
        return 0;
    sym->name = dup_string(name ? name : "");
    if (!sym->name) {
        free(sym);
        return 0;
    }
    sym->type = type;
    sym->array_size = array_size;
    sym->param_index = -1;
    sym->param_types = NULL;
    sym->param_count = 0;
    sym->next = table->globals;
    table->globals = sym;
    return 1;
}

int symtable_add_func(symtable_t *table, const char *name, type_kind_t ret_type,
                      type_kind_t *param_types, size_t param_count)
{
    if (symtable_lookup(table, name))
        return 0;
    symbol_t *sym = malloc(sizeof(*sym));
    if (!sym)
        return 0;
    sym->name = dup_string(name ? name : "");
    if (!sym->name) {
        free(sym);
        return 0;
    }
    sym->type = ret_type;
    sym->param_index = -1;
    sym->param_count = param_count;
    sym->param_types = NULL;
    if (param_count) {
        sym->param_types = malloc(param_count * sizeof(*sym->param_types));
        if (!sym->param_types) {
            free(sym->name);
            free(sym);
            return 0;
        }
        for (size_t i = 0; i < param_count; i++)
            sym->param_types[i] = param_types[i];
    }
    sym->next = table->head;
    table->head = sym;
    return 1;
}

symbol_t *symtable_lookup_global(symtable_t *table, const char *name)
{
    for (symbol_t *sym = table->globals; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0)
            return sym;
    }
    return NULL;
}

/* Evaluate a constant expression at compile time. Returns non-zero on success. */
static int eval_const_expr(expr_t *expr, int *out)
{
    if (!expr)
        return 0;
    switch (expr->kind) {
    case EXPR_NUMBER:
        if (out)
            *out = (int)strtol(expr->number.value, NULL, 10);
        return 1;
    case EXPR_CHAR:
        if (out)
            *out = (int)expr->ch.value;
        return 1;
    case EXPR_UNARY:
        if (expr->unary.op == UNOP_NEG) {
            int val;
            if (eval_const_expr(expr->unary.operand, &val)) {
                if (out)
                    *out = -val;
                return 1;
            }
        }
        return 0;
    case EXPR_BINARY: {
        int a, b;
        if (!eval_const_expr(expr->binary.left, &a) ||
            !eval_const_expr(expr->binary.right, &b))
            return 0;
        switch (expr->binary.op) {
        case BINOP_ADD: if (out) *out = a + b; break;
        case BINOP_SUB: if (out) *out = a - b; break;
        case BINOP_MUL: if (out) *out = a * b; break;
        case BINOP_DIV: if (out) *out = b != 0 ? a / b : 0; break;
        case BINOP_EQ:  if (out) *out = (a == b); break;
        case BINOP_NEQ: if (out) *out = (a != b); break;
        case BINOP_LT:  if (out) *out = (a < b); break;
        case BINOP_GT:  if (out) *out = (a > b); break;
        case BINOP_LE:  if (out) *out = (a <= b); break;
        case BINOP_GE:  if (out) *out = (a >= b); break;
        default:
            return 0;
        }
        return 1;
    }
    default:
        return 0;
    }
}

static type_kind_t check_binary(expr_t *left, expr_t *right, symtable_t *vars,
                                symtable_t *funcs, ir_builder_t *ir,
                                ir_value_t *out, binop_t op)
{
    ir_value_t lval, rval;
    type_kind_t lt = check_expr(left, vars, funcs, ir, &lval);
    type_kind_t rt = check_expr(right, vars, funcs, ir, &rval);
    if (lt == TYPE_INT && rt == TYPE_INT) {
        if (out) {
            ir_op_t ir_op;
            switch (op) {
            case BINOP_ADD: ir_op = IR_ADD; break;
            case BINOP_SUB: ir_op = IR_SUB; break;
            case BINOP_MUL: ir_op = IR_MUL; break;
            case BINOP_DIV: ir_op = IR_DIV; break;
            case BINOP_EQ:  ir_op = IR_CMPEQ; break;
            case BINOP_NEQ: ir_op = IR_CMPNE; break;
            case BINOP_LT:  ir_op = IR_CMPLT; break;
            case BINOP_GT:  ir_op = IR_CMPGT; break;
            case BINOP_LE:  ir_op = IR_CMPLE; break;
            case BINOP_GE:  ir_op = IR_CMPGE; break;
            }
            *out = ir_build_binop(ir, ir_op, lval, rval);
        }
        return TYPE_INT;
    }
    set_error(left->line, left->column);
    return TYPE_UNKNOWN;
}

type_kind_t check_expr(expr_t *expr, symtable_t *vars, symtable_t *funcs,
                       ir_builder_t *ir, ir_value_t *out)
{
    if (!expr)
        return TYPE_UNKNOWN;
    switch (expr->kind) {
    case EXPR_NUMBER:
        if (out)
            *out = ir_build_const(ir, (int)strtol(expr->number.value, NULL, 10));
        return TYPE_INT;
    case EXPR_STRING:
        if (out)
            *out = ir_build_string(ir, expr->string.value);
        return TYPE_INT;
    case EXPR_CHAR:
        if (out)
            *out = ir_build_const(ir, (int)expr->ch.value);
        return TYPE_INT;
    case EXPR_UNARY:
        if (expr->unary.op == UNOP_DEREF) {
            ir_value_t addr;
            if (check_expr(expr->unary.operand, vars, funcs, ir, &addr) == TYPE_PTR) {
                if (out)
                    *out = ir_build_load_ptr(ir, addr);
                return TYPE_INT;
            }
            set_error(expr->unary.operand->line, expr->unary.operand->column);
            return TYPE_UNKNOWN;
        } else if (expr->unary.op == UNOP_ADDR) {
            if (expr->unary.operand->kind != EXPR_IDENT) {
                set_error(expr->unary.operand->line, expr->unary.operand->column);
                return TYPE_UNKNOWN;
            }
            symbol_t *sym = symtable_lookup(vars, expr->unary.operand->ident.name);
            if (!sym) {
                set_error(expr->unary.operand->line, expr->unary.operand->column);
                return TYPE_UNKNOWN;
            }
            if (out)
                *out = ir_build_addr(ir, sym->name);
            return TYPE_PTR;
        } else if (expr->unary.op == UNOP_NEG) {
            ir_value_t val;
            if (check_expr(expr->unary.operand, vars, funcs, ir, &val) == TYPE_INT) {
                if (out) {
                    ir_value_t zero = ir_build_const(ir, 0);
                    *out = ir_build_binop(ir, IR_SUB, zero, val);
                }
                return TYPE_INT;
            }
            set_error(expr->unary.operand->line, expr->unary.operand->column);
            return TYPE_UNKNOWN;
        }
        return TYPE_UNKNOWN;
    case EXPR_IDENT: {
        symbol_t *sym = symtable_lookup(vars, expr->ident.name);
        if (!sym) {
            set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        if (sym->type == TYPE_ARRAY) {
            if (out)
                *out = ir_build_addr(ir, sym->name);
            return TYPE_PTR;
        } else {
            if (out) {
                if (sym->param_index >= 0)
                    *out = ir_build_load_param(ir, sym->param_index);
                else
                    *out = ir_build_load(ir, expr->ident.name);
            }
            return sym->type;
        }
    }
    case EXPR_BINARY:
        return check_binary(expr->binary.left, expr->binary.right, vars, funcs,
                           ir, out, expr->binary.op);
    case EXPR_ASSIGN: {
        ir_value_t val;
        symbol_t *sym = symtable_lookup(vars, expr->assign.name);
        if (!sym) {
            set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        if (check_expr(expr->assign.value, vars, funcs, ir, &val) == sym->type) {
            if (sym->param_index >= 0)
                ir_build_store_param(ir, sym->param_index, val);
            else
                ir_build_store(ir, expr->assign.name, val);
            if (out)
                *out = val;
            return sym->type;
        }
        set_error(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    case EXPR_INDEX: {
        if (expr->index.array->kind != EXPR_IDENT) {
            set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        symbol_t *sym = symtable_lookup(vars, expr->index.array->ident.name);
        if (!sym || sym->type != TYPE_ARRAY) {
            set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        ir_value_t idx_val;
        if (check_expr(expr->index.index, vars, funcs, ir, &idx_val) != TYPE_INT) {
            set_error(expr->index.index->line, expr->index.index->column);
            return TYPE_UNKNOWN;
        }
        int cval;
        if (sym->array_size && eval_const_expr(expr->index.index, &cval)) {
            if (cval < 0 || (size_t)cval >= sym->array_size) {
                set_error(expr->index.index->line, expr->index.index->column);
                return TYPE_UNKNOWN;
            }
        }
        if (out)
            *out = ir_build_load_idx(ir, sym->name, idx_val);
        return TYPE_INT;
    }
    case EXPR_ASSIGN_INDEX: {
        if (expr->assign_index.array->kind != EXPR_IDENT) {
            set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        symbol_t *sym = symtable_lookup(vars, expr->assign_index.array->ident.name);
        if (!sym || sym->type != TYPE_ARRAY) {
            set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        ir_value_t idx_val, val;
        if (check_expr(expr->assign_index.index, vars, funcs, ir, &idx_val) != TYPE_INT) {
            set_error(expr->assign_index.index->line, expr->assign_index.index->column);
            return TYPE_UNKNOWN;
        }
        if (check_expr(expr->assign_index.value, vars, funcs, ir, &val) != TYPE_INT) {
            set_error(expr->assign_index.value->line, expr->assign_index.value->column);
            return TYPE_UNKNOWN;
        }
        int cval;
        if (sym->array_size && eval_const_expr(expr->assign_index.index, &cval)) {
            if (cval < 0 || (size_t)cval >= sym->array_size) {
                set_error(expr->assign_index.index->line, expr->assign_index.index->column);
                return TYPE_UNKNOWN;
            }
        }
        ir_build_store_idx(ir, sym->name, idx_val, val);
        if (out)
            *out = val;
        return TYPE_INT;
    }
    case EXPR_CALL: {
        symbol_t *fsym = symtable_lookup(funcs, expr->call.name);
        if (!fsym) {
            set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        if (fsym->param_count != expr->call.arg_count) {
            set_error(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        ir_value_t *vals = NULL;
        if (expr->call.arg_count) {
            vals = malloc(expr->call.arg_count * sizeof(*vals));
            if (!vals)
                return TYPE_UNKNOWN;
        }
        for (size_t i = 0; i < expr->call.arg_count; i++) {
            type_kind_t at = check_expr(expr->call.args[i], vars, funcs, ir,
                                        &vals[i]);
            if (at != fsym->param_types[i]) {
                set_error(expr->call.args[i]->line, expr->call.args[i]->column);
                free(vals);
                return TYPE_UNKNOWN;
            }
        }
        for (size_t i = expr->call.arg_count; i > 0; i--)
            ir_build_arg(ir, vals[i - 1]);
        free(vals);
        ir_value_t call_val = ir_build_call(ir, expr->call.name,
                                           expr->call.arg_count);
        if (out)
            *out = call_val;
        return fsym->type;
    }
    }
    set_error(expr->line, expr->column);
    return TYPE_UNKNOWN;
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
                set_error(stmt->line, stmt->column);
                return 0;
            }
            ir_value_t zero = ir_build_const(ir, 0);
            ir_build_return(ir, zero);
            return 1;
        }
        ir_value_t val;
        if (check_expr(stmt->ret.expr, vars, funcs, ir, &val) == TYPE_UNKNOWN) {
            set_error(stmt->ret.expr->line, stmt->ret.expr->column);
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
        int id = next_label_id++;
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
        int id = next_label_id++;
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
    case STMT_FOR: {
        ir_value_t cond_val;
        char start_label[32];
        char end_label[32];
        int id = next_label_id++;
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
            set_error(stmt->line, stmt->column);
            return 0;
        }
        ir_build_br(ir, break_label);
        return 1;
    case STMT_CONTINUE:
        if (!continue_label) {
            set_error(stmt->line, stmt->column);
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
            set_error(stmt->line, stmt->column);
            return 0;
        }
        if (stmt->var_decl.init) {
            ir_value_t val;
            if (check_expr(stmt->var_decl.init, vars, funcs, ir, &val) !=
                stmt->var_decl.type) {
                set_error(stmt->var_decl.init->line, stmt->var_decl.init->column);
                return 0;
            }
            ir_build_store(ir, stmt->var_decl.name, val);
        }
        return 1;
    }
    }
    return 0;
}

int check_func(func_t *func, symtable_t *funcs, symtable_t *globals,
               ir_builder_t *ir)
{
    if (!func)
        return 0;

    symtable_t locals;
    symtable_init(&locals);
    locals.globals = globals ? globals->globals : NULL;

    for (size_t i = 0; i < func->param_count; i++)
        symtable_add_param(&locals, func->param_names[i],
                           func->param_types[i], (int)i);

    ir_build_func_begin(ir, func->name);

    int ok = 1;
    for (size_t i = 0; i < func->body_count && ok; i++)
        ok = check_stmt(func->body[i], &locals, funcs, ir, func->return_type,
                        NULL, NULL);

    ir_build_func_end(ir);

    locals.globals = NULL;
    symtable_free(&locals);
    return ok;
}

int check_global(stmt_t *decl, symtable_t *globals, ir_builder_t *ir)
{
    if (!decl || decl->kind != STMT_VAR_DECL)
        return 0;
    if (!symtable_add_global(globals, decl->var_decl.name,
                             decl->var_decl.type,
                             decl->var_decl.array_size)) {
        set_error(decl->line, decl->column);
        return 0;
    }
    int value = 0;
    if (decl->var_decl.init) {
        if (!eval_const_expr(decl->var_decl.init, &value)) {
            set_error(decl->var_decl.init->line, decl->var_decl.init->column);
            return 0;
        }
    }
    ir_build_glob_var(ir, decl->var_decl.name, value);
    return 1;
}

