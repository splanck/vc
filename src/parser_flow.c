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
#include "ast_stmt.h"
#include "ast_expr.h"
#include "parser_decl_var.h"

/* Parse an if statement starting at the 'if' keyword. */
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

/* Parse a while loop beginning with the 'while' keyword. */
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

/* Parse a do-while loop starting at the 'do' keyword. */
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

/* Parse a for loop beginning with the 'for' keyword. */
stmt_t *parser_parse_for_stmt(parser_t *p)
{
    if (!match(p, TOK_KW_FOR))
        return NULL;
    token_t *kw_tok = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LPAREN))
        return NULL;
    stmt_t *init_decl = NULL;
    expr_t *init = NULL;
    size_t save = p->pos;
    init_decl = parser_parse_var_decl(p);
    if (!init_decl) {
        p->pos = save;
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

/* Parse a single 'case' clause and store result in out. */
static int parse_single_case(parser_t *p, switch_case_t *out)
{
    expr_t *val = parser_parse_expr(p);
    if (!val || !match(p, TOK_COLON)) {
        ast_free_expr(val);
        return 0;
    }
    stmt_t *body = parser_parse_stmt(p);
    if (!body) {
        ast_free_expr(val);
        return 0;
    }
    out->expr = val;
    out->body = body;
    return 1;
}

/* Parse a 'default' clause and store the resulting statement. */
static int parse_default_case(parser_t *p, stmt_t **body)
{
    if (!match(p, TOK_COLON))
        return 0;
    *body = parser_parse_stmt(p);
    if (!*body)
        return 0;
    return 1;
}

/* Parse the list of case/default clauses inside a switch block. */
static int parse_switch_cases(parser_t *p, switch_case_t **cases,
                              size_t *count, stmt_t **default_body)
{
    vector_t cases_v;
    vector_init(&cases_v, sizeof(switch_case_t));
    *default_body = NULL;
    while (!match(p, TOK_RBRACE)) {
        if (match(p, TOK_KW_CASE)) {
            switch_case_t tmp;
            if (!parse_single_case(p, &tmp))
                goto error;
            if (!vector_push(&cases_v, &tmp)) {
                ast_free_expr(tmp.expr);
                ast_free_stmt(tmp.body);
                goto error;
            }
        } else if (match(p, TOK_KW_DEFAULT)) {
            if (*default_body || !parse_default_case(p, default_body))
                goto error;
        } else {
            goto error;
        }
    }
    *cases = (switch_case_t *)cases_v.data;
    *count = cases_v.count;
    return 1;
error:
    for (size_t i = 0; i < cases_v.count; i++) {
        switch_case_t *c = &((switch_case_t *)cases_v.data)[i];
        ast_free_expr(c->expr);
        ast_free_stmt(c->body);
    }
    vector_free(&cases_v);
    return 0;
}

/* Parse a switch statement starting at the 'switch' keyword. */
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
    switch_case_t *cases = NULL;
    size_t case_count = 0;
    stmt_t *default_body = NULL;
    if (!parse_switch_cases(p, &cases, &case_count, &default_body)) {
        ast_free_expr(expr);
        return NULL;
    }
    stmt_t *stmt = ast_make_switch(expr, cases, case_count, default_body,
                                   kw_tok->line, kw_tok->column);
    if (!stmt) {
        for (size_t i = 0; i < case_count; i++) {
            ast_free_expr(cases[i].expr);
            ast_free_stmt(cases[i].body);
        }
        free(cases);
        ast_free_expr(expr);
        ast_free_stmt(default_body);
        return NULL;
    }
    return stmt;
}

