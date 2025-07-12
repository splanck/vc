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
    ast_free_expr(STMT_EXPR(stmt).expr);
}

static void free_return_stmt(stmt_t *stmt)
{
    ast_free_expr(STMT_RET(stmt).expr);
}

static void free_var_decl_stmt(stmt_t *stmt)
{
    free(STMT_VAR_DECL(stmt).name);
    ast_free_expr(STMT_VAR_DECL(stmt).size_expr);
    ast_free_expr(STMT_VAR_DECL(stmt).align_expr);
    ast_free_expr(STMT_VAR_DECL(stmt).init);
    for (size_t i = 0; i < STMT_VAR_DECL(stmt).init_count; i++) {
        ast_free_expr(STMT_VAR_DECL(stmt).init_list[i].index);
        ast_free_expr(STMT_VAR_DECL(stmt).init_list[i].value);
        free(STMT_VAR_DECL(stmt).init_list[i].field);
    }
    free(STMT_VAR_DECL(stmt).init_list);
    free(STMT_VAR_DECL(stmt).tag);
    for (size_t i = 0; i < STMT_VAR_DECL(stmt).member_count; i++)
        free(STMT_VAR_DECL(stmt).members[i].name);
    free(STMT_VAR_DECL(stmt).members);
    free(STMT_VAR_DECL(stmt).func_param_types);
}

static void free_if_stmt(stmt_t *stmt)
{
    ast_free_expr(STMT_IF(stmt).cond);
    ast_free_stmt(STMT_IF(stmt).then_branch);
    ast_free_stmt(STMT_IF(stmt).else_branch);
}

static void free_while_stmt(stmt_t *stmt)
{
    ast_free_expr(STMT_WHILE(stmt).cond);
    ast_free_stmt(STMT_WHILE(stmt).body);
}

static void free_do_while_stmt(stmt_t *stmt)
{
    ast_free_expr(STMT_DO_WHILE(stmt).cond);
    ast_free_stmt(STMT_DO_WHILE(stmt).body);
}

static void free_for_stmt(stmt_t *stmt)
{
    ast_free_stmt(STMT_FOR(stmt).init_decl);
    ast_free_expr(STMT_FOR(stmt).init);
    ast_free_expr(STMT_FOR(stmt).cond);
    ast_free_expr(STMT_FOR(stmt).incr);
    ast_free_stmt(STMT_FOR(stmt).body);
}

static void free_switch_stmt(stmt_t *stmt)
{
    ast_free_expr(STMT_SWITCH(stmt).expr);
    for (size_t i = 0; i < STMT_SWITCH(stmt).case_count; i++) {
        ast_free_expr(STMT_SWITCH(stmt).cases[i].expr);
        ast_free_stmt(STMT_SWITCH(stmt).cases[i].body);
    }
    free(STMT_SWITCH(stmt).cases);
    ast_free_stmt(STMT_SWITCH(stmt).default_body);
}

static void free_label_stmt(stmt_t *stmt)
{
    free(STMT_LABEL(stmt).name);
}

static void free_goto_stmt(stmt_t *stmt)
{
    free(STMT_GOTO(stmt).name);
}

static void free_static_assert_stmt(stmt_t *stmt)
{
    ast_free_expr(STMT_STATIC_ASSERT(stmt).expr);
    free(STMT_STATIC_ASSERT(stmt).message);
}

static void free_typedef_stmt(stmt_t *stmt)
{
    free(STMT_TYPEDEF(stmt).name);
}

static void free_enum_decl_stmt(stmt_t *stmt)
{
    free(STMT_ENUM_DECL(stmt).tag);
    for (size_t i = 0; i < STMT_ENUM_DECL(stmt).count; i++) {
        free(STMT_ENUM_DECL(stmt).items[i].name);
        ast_free_expr(STMT_ENUM_DECL(stmt).items[i].value);
    }
    free(STMT_ENUM_DECL(stmt).items);
}

static void free_struct_decl_stmt(stmt_t *stmt)
{
    free(STMT_STRUCT_DECL(stmt).tag);
    for (size_t i = 0; i < STMT_STRUCT_DECL(stmt).count; i++)
        free(STMT_STRUCT_DECL(stmt).members[i].name);
    free(STMT_STRUCT_DECL(stmt).members);
}

static void free_union_decl_stmt(stmt_t *stmt)
{
    free(STMT_UNION_DECL(stmt).tag);
    for (size_t i = 0; i < STMT_UNION_DECL(stmt).count; i++)
        free(STMT_UNION_DECL(stmt).members[i].name);
    free(STMT_UNION_DECL(stmt).members);
}

static void free_block_stmt(stmt_t *stmt)
{
    for (size_t i = 0; i < STMT_BLOCK(stmt).count; i++)
        ast_free_stmt(STMT_BLOCK(stmt).stmts[i]);
    free(STMT_BLOCK(stmt).stmts);
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

