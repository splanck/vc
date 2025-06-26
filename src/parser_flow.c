/*
 * Parsing for control flow statements.
 *
 * Contains helpers for "if", "while", "do-while", "for" and "switch"
 * constructs.  Each function begins parsing at the keyword introducing
 * the statement and returns the resulting stmt_t on success.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "parser.h"
#include "vector.h"

/* Forward declaration from parser_decl.c */
stmt_t *parser_parse_var_decl(parser_t *p);

stmt_t *parser_parse_if_stmt(parser_t *p)
{
    if (!match(p, TOK_KW_IF))
        return NULL;
    token_t *kw_tok = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LPAREN))
        return NULL;
    expr_t *cond = parser_parse_expr(p);
    if (!cond || !match(p, TOK_RPAREN)) {
        ast_free_expr(cond);
        return NULL;
    }
    stmt_t *then_branch = parser_parse_stmt(p);
    if (!then_branch) {
        ast_free_expr(cond);
        return NULL;
    }
    stmt_t *else_branch = NULL;
    if (match(p, TOK_KW_ELSE)) {
        else_branch = parser_parse_stmt(p);
        if (!else_branch) {
            ast_free_expr(cond);
            ast_free_stmt(then_branch);
            return NULL;
        }
    }
    return ast_make_if(cond, then_branch, else_branch,
                       kw_tok->line, kw_tok->column);
}

stmt_t *parser_parse_while_stmt(parser_t *p)
{
    if (!match(p, TOK_KW_WHILE))
        return NULL;
    token_t *kw_tok = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LPAREN))
        return NULL;
    expr_t *cond = parser_parse_expr(p);
    if (!cond || !match(p, TOK_RPAREN)) {
        ast_free_expr(cond);
        return NULL;
    }
    stmt_t *body = parser_parse_stmt(p);
    if (!body) {
        ast_free_expr(cond);
        return NULL;
    }
    return ast_make_while(cond, body, kw_tok->line, kw_tok->column);
}

stmt_t *parser_parse_do_while_stmt(parser_t *p)
{
    if (!match(p, TOK_KW_DO))
        return NULL;
    token_t *kw_tok = &p->tokens[p->pos - 1];
    stmt_t *body = parser_parse_stmt(p);
    if (!body)
        return NULL;
    if (!match(p, TOK_KW_WHILE)) {
        ast_free_stmt(body);
        return NULL;
    }
    if (!match(p, TOK_LPAREN)) {
        ast_free_stmt(body);
        return NULL;
    }
    expr_t *cond = parser_parse_expr(p);
    if (!cond || !match(p, TOK_RPAREN)) {
        ast_free_stmt(body);
        ast_free_expr(cond);
        return NULL;
    }
    if (!match(p, TOK_SEMI)) {
        ast_free_stmt(body);
        ast_free_expr(cond);
        return NULL;
    }
    return ast_make_do_while(cond, body, kw_tok->line, kw_tok->column);
}

stmt_t *parser_parse_for_stmt(parser_t *p)
{
    if (!match(p, TOK_KW_FOR))
        return NULL;
    token_t *kw_tok = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LPAREN))
        return NULL;
    stmt_t *init_decl = NULL;
    expr_t *init = NULL;
    token_t *tok = peek(p);
    if (tok && (tok->type == TOK_KW_STATIC || tok->type == TOK_KW_CONST ||
                tok->type == TOK_KW_INT || tok->type == TOK_KW_CHAR ||
                tok->type == TOK_KW_FLOAT || tok->type == TOK_KW_DOUBLE ||
                tok->type == TOK_KW_SHORT || tok->type == TOK_KW_LONG ||
                tok->type == TOK_KW_BOOL || tok->type == TOK_KW_UNSIGNED)) {
        init_decl = parser_parse_var_decl(p);
        if (!init_decl)
            return NULL;
    } else {
        init = parser_parse_expr(p);
        if (!init || !match(p, TOK_SEMI)) {
            ast_free_expr(init);
            return NULL;
        }
    }
    expr_t *cond = parser_parse_expr(p);
    if (!cond || !match(p, TOK_SEMI)) {
        ast_free_stmt(init_decl);
        ast_free_expr(init);
        ast_free_expr(cond);
        return NULL;
    }
    expr_t *incr = parser_parse_expr(p);
    if (!incr || !match(p, TOK_RPAREN)) {
        ast_free_stmt(init_decl);
        ast_free_expr(init);
        ast_free_expr(cond);
        ast_free_expr(incr);
        return NULL;
    }
    stmt_t *body = parser_parse_stmt(p);
    if (!body) {
        ast_free_stmt(init_decl);
        ast_free_expr(init);
        ast_free_expr(cond);
        ast_free_expr(incr);
        return NULL;
    }
    return ast_make_for(init_decl, init, cond, incr, body,
                        kw_tok->line, kw_tok->column);
}

stmt_t *parser_parse_switch_stmt(parser_t *p)
{
    if (!match(p, TOK_KW_SWITCH))
        return NULL;
    token_t *kw_tok = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LPAREN))
        return NULL;
    expr_t *expr = parser_parse_expr(p);
    if (!expr || !match(p, TOK_RPAREN)) {
        ast_free_expr(expr);
        return NULL;
    }
    if (!match(p, TOK_LBRACE)) {
        ast_free_expr(expr);
        return NULL;
    }
    vector_t cases_v;
    vector_init(&cases_v, sizeof(switch_case_t));
    stmt_t *default_body = NULL;
    while (!match(p, TOK_RBRACE)) {
        if (match(p, TOK_KW_CASE)) {
            expr_t *val = parser_parse_expr(p);
            if (!val || !match(p, TOK_COLON)) {
                ast_free_expr(val);
                goto error_switch;
            }
            stmt_t *body = parser_parse_stmt(p);
            if (!body) {
                ast_free_expr(val);
                goto error_switch;
            }
            switch_case_t tmp = { val, body };
            if (!vector_push(&cases_v, &tmp)) {
                ast_free_expr(val);
                ast_free_stmt(body);
                goto error_switch;
            }
        } else if (match(p, TOK_KW_DEFAULT)) {
            if (default_body) {
                goto error_switch;
            }
            if (!match(p, TOK_COLON))
                goto error_switch;
            default_body = parser_parse_stmt(p);
            if (!default_body)
                goto error_switch;
        } else {
            goto error_switch;
        }
    }
    {
        size_t count = cases_v.count;
        switch_case_t *cases = (switch_case_t *)cases_v.data;
        stmt_t *stmt = ast_make_switch(expr, cases, count, default_body,
                                      kw_tok->line, kw_tok->column);
        if (!stmt)
            goto error_switch;
        return stmt;
    }
error_switch:
    for (size_t i = 0; i < cases_v.count; i++) {
        switch_case_t *c = &((switch_case_t *)cases_v.data)[i];
        ast_free_expr(c->expr);
        ast_free_stmt(c->body);
    }
    free(cases_v.data);
    ast_free_expr(expr);
    ast_free_stmt(default_body);
    return NULL;
}

