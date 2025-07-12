/*
 * Statement and function AST destructors for the compiler.
 *
 * This file implements `ast_free_stmt` and `ast_free_func` along with
 * helper routines for freeing individual statement kinds.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */
#include <stdlib.h>
#include "ast_stmt.h"
#include "ast_expr.h"
#include "util.h"

/* Helpers for freeing individual statement types */
static void free_expr_stmt(stmt_t *stmt)
{
    ast_free_expr(stmt->data.expr.expr);
}

static void free_return_stmt(stmt_t *stmt)
{
    ast_free_expr(stmt->data.ret.expr);
}

static void free_var_decl_stmt(stmt_t *stmt)
{
    free(stmt->data.var_decl.name);
    ast_free_expr(stmt->data.var_decl.size_expr);
    ast_free_expr(stmt->data.var_decl.align_expr);
    ast_free_expr(stmt->data.var_decl.init);
    for (size_t i = 0; i < stmt->data.var_decl.init_count; i++) {
        ast_free_expr(stmt->data.var_decl.init_list[i].index);
        ast_free_expr(stmt->data.var_decl.init_list[i].value);
        free(stmt->data.var_decl.init_list[i].field);
    }
    free(stmt->data.var_decl.init_list);
    free(stmt->data.var_decl.tag);
    for (size_t i = 0; i < stmt->data.var_decl.member_count; i++)
        free(stmt->data.var_decl.members[i].name);
    free(stmt->data.var_decl.members);
    free(stmt->data.var_decl.func_param_types);
}

static void free_if_stmt(stmt_t *stmt)
{
    ast_free_expr(stmt->data.if_stmt.cond);
    ast_free_stmt(stmt->data.if_stmt.then_branch);
    ast_free_stmt(stmt->data.if_stmt.else_branch);
}

static void free_while_stmt(stmt_t *stmt)
{
    ast_free_expr(stmt->data.while_stmt.cond);
    ast_free_stmt(stmt->data.while_stmt.body);
}

static void free_do_while_stmt(stmt_t *stmt)
{
    ast_free_expr(stmt->data.do_while_stmt.cond);
    ast_free_stmt(stmt->data.do_while_stmt.body);
}

static void free_for_stmt(stmt_t *stmt)
{
    ast_free_stmt(stmt->data.for_stmt.init_decl);
    ast_free_expr(stmt->data.for_stmt.init);
    ast_free_expr(stmt->data.for_stmt.cond);
    ast_free_expr(stmt->data.for_stmt.incr);
    ast_free_stmt(stmt->data.for_stmt.body);
}

static void free_switch_stmt(stmt_t *stmt)
{
    ast_free_expr(stmt->data.switch_stmt.expr);
    for (size_t i = 0; i < stmt->data.switch_stmt.case_count; i++) {
        ast_free_expr(stmt->data.switch_stmt.cases[i].expr);
        ast_free_stmt(stmt->data.switch_stmt.cases[i].body);
    }
    free(stmt->data.switch_stmt.cases);
    ast_free_stmt(stmt->data.switch_stmt.default_body);
}

static void free_label_stmt(stmt_t *stmt)
{
    free(stmt->data.label.name);
}

static void free_goto_stmt(stmt_t *stmt)
{
    free(stmt->data.goto_stmt.name);
}

static void free_static_assert_stmt(stmt_t *stmt)
{
    ast_free_expr(stmt->data.static_assert.expr);
    free(stmt->data.static_assert.message);
}

static void free_typedef_stmt(stmt_t *stmt)
{
    free(stmt->data.typedef_decl.name);
}

static void free_enum_decl_stmt(stmt_t *stmt)
{
    free(stmt->data.enum_decl.tag);
    for (size_t i = 0; i < stmt->data.enum_decl.count; i++) {
        free(stmt->data.enum_decl.items[i].name);
        ast_free_expr(stmt->data.enum_decl.items[i].value);
    }
    free(stmt->data.enum_decl.items);
}

static void free_struct_decl_stmt(stmt_t *stmt)
{
    free(stmt->data.struct_decl.tag);
    for (size_t i = 0; i < stmt->data.struct_decl.count; i++)
        free(stmt->data.struct_decl.members[i].name);
    free(stmt->data.struct_decl.members);
}

static void free_union_decl_stmt(stmt_t *stmt)
{
    free(stmt->data.union_decl.tag);
    for (size_t i = 0; i < stmt->data.union_decl.count; i++)
        free(stmt->data.union_decl.members[i].name);
    free(stmt->data.union_decl.members);
}

static void free_block_stmt(stmt_t *stmt)
{
    for (size_t i = 0; i < stmt->data.block.count; i++)
        ast_free_stmt(stmt->data.block.stmts[i]);
    free(stmt->data.block.stmts);
}

static void free_break_stmt(stmt_t *stmt)
{
    (void)stmt;
}

static void free_continue_stmt(stmt_t *stmt)
{
    (void)stmt;
}
/* Free a statement node and all of its children. */
void ast_free_stmt(stmt_t *stmt)
{
    if (!stmt)
        return;
    switch (stmt->kind) {
    case STMT_EXPR:
        free_expr_stmt(stmt);
        break;
    case STMT_RETURN:
        free_return_stmt(stmt);
        break;
    case STMT_VAR_DECL:
        free_var_decl_stmt(stmt);
        break;
    case STMT_IF:
        free_if_stmt(stmt);
        break;
    case STMT_WHILE:
        free_while_stmt(stmt);
        break;
    case STMT_DO_WHILE:
        free_do_while_stmt(stmt);
        break;
    case STMT_FOR:
        free_for_stmt(stmt);
        break;
    case STMT_SWITCH:
        free_switch_stmt(stmt);
        break;
    case STMT_LABEL:
        free_label_stmt(stmt);
        break;
    case STMT_GOTO:
        free_goto_stmt(stmt);
        break;
    case STMT_STATIC_ASSERT:
        free_static_assert_stmt(stmt);
        break;
    case STMT_TYPEDEF:
        free_typedef_stmt(stmt);
        break;
    case STMT_ENUM_DECL:
        free_enum_decl_stmt(stmt);
        break;
    case STMT_STRUCT_DECL:
        free_struct_decl_stmt(stmt);
        break;
    case STMT_UNION_DECL:
        free_union_decl_stmt(stmt);
        break;
    case STMT_BREAK:
        free_break_stmt(stmt);
        break;
    case STMT_CONTINUE:
        free_continue_stmt(stmt);
        break;
    case STMT_BLOCK:
        free_block_stmt(stmt);
        break;
    }
    free(stmt);
}

/* Free a function definition and its entire body. */
void ast_free_func(func_t *func)
{
    if (!func)
        return;
    for (size_t i = 0; i < func->body_count; i++)
        ast_free_stmt(func->body[i]);
    free(func->body);
    for (size_t i = 0; i < func->param_count; i++)
        free(func->param_names[i]);
    for (size_t i = 0; i < func->param_count; i++)
        free(func->param_tags[i]);
    free(func->param_names);
    free(func->param_types);
    free(func->param_tags);
    free(func->param_elem_sizes);
    free(func->param_is_restrict);
    free(func->return_tag);
    free(func->name);
    free(func);
}

