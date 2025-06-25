/*
 * Statement parser for the language.
 *
 * Functions in this file build AST nodes for the various statement
 * forms (blocks, loops, if/else, declarations, etc.).  Each routine
 * consumes the tokens belonging to the construct and returns the newly
 * created stmt_t on success.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "parser.h"
#include "vector.h"
#include "util.h"

static stmt_t *parse_block(parser_t *p);
stmt_t *parser_parse_enum_decl(parser_t *p);
stmt_t *parser_parse_stmt(parser_t *p);

/* Parse a "{...}" block recursively collecting inner statements. */
static stmt_t *parse_block(parser_t *p)
{
    if (!match(p, TOK_LBRACE))
        return NULL;
    token_t *lb_tok = &p->tokens[p->pos - 1];
    size_t cap = 4, count = 0;
    stmt_t **stmts = malloc(cap * sizeof(*stmts));
    if (!stmts)
        return NULL;
    while (!match(p, TOK_RBRACE)) {
        stmt_t *s = parser_parse_stmt(p);
        if (!s) {
            for (size_t i = 0; i < count; i++)
                ast_free_stmt(stmts[i]);
            free(stmts);
            return NULL;
        }
        if (count >= cap) {
            cap *= 2;
            stmt_t **tmp = realloc(stmts, cap * sizeof(*tmp));
            if (!tmp) {
                for (size_t i = 0; i < count; i++)
                    ast_free_stmt(stmts[i]);
                free(stmts);
                return NULL;
            }
            stmts = tmp;
        }
        stmts[count++] = s;
    }
    return ast_make_block(stmts, count, lb_tok->line, lb_tok->column);
}

/* Parse an enum declaration */
stmt_t *parser_parse_enum_decl(parser_t *p)
{
    token_t *kw = &p->tokens[p->pos - 1];
    token_t *tok = peek(p);
    char *tag = NULL;
    if (tok && tok->type == TOK_IDENT) {
        p->pos++;
        tag = tok->lexeme;
    }
    if (!match(p, TOK_LBRACE))
        return NULL;

    size_t cap = 4, count = 0;
    enumerator_t *items = malloc(cap * sizeof(*items));
    if (!items)
        return NULL;
    int ok = 0;
    do {
        tok = peek(p);
        if (!tok || tok->type != TOK_IDENT)
            goto fail;
        p->pos++;
        char *name = vc_strdup(tok->lexeme);
        if (!name)
            goto fail;
        expr_t *val = NULL;
        if (match(p, TOK_ASSIGN)) {
            val = parser_parse_expr(p);
            if (!val)
                goto fail;
        }
        if (count >= cap) {
            cap *= 2;
            enumerator_t *tmp = realloc(items, cap * sizeof(*tmp));
            if (!tmp)
                goto fail;
            items = tmp;
        }
        items[count].name = name;
        items[count].value = val;
        count++;
    } while (match(p, TOK_COMMA));

    if (!match(p, TOK_RBRACE) || !match(p, TOK_SEMI))
        goto fail;

    ok = 1;
fail:
    if (!ok) {
        for (size_t i = 0; i < count; i++) {
            free(items[i].name);
            ast_free_expr(items[i].value);
        }
        free(items);
        return NULL;
    }
    return ast_make_enum_decl(tag, items, count, kw->line, kw->column);
}

/*
 * Parse a single statement at the current position.  This function
 * delegates to the specific helpers for declarations, control flow
 * constructs and expression statements.  It returns the constructed
 * stmt_t or NULL on failure.
 */
stmt_t *parser_parse_stmt(parser_t *p)
{
    token_t *tok = peek(p);
    if (tok && tok->type == TOK_LBRACE)
        return parse_block(p);

    if (match(p, TOK_LABEL)) {
        token_t *lbl = &p->tokens[p->pos - 1];
        return ast_make_label(lbl->lexeme, lbl->line, lbl->column);
    }

    if (match(p, TOK_KW_ENUM))
        return parser_parse_enum_decl(p);

    if (match(p, TOK_KW_INT) || match(p, TOK_KW_CHAR)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        type_kind_t t = (kw_tok->type == TOK_KW_INT) ? TYPE_INT : TYPE_CHAR;
        if (match(p, TOK_STAR))
            t = TYPE_PTR;
        token_t *tok = peek(p);
        if (!tok || tok->type != TOK_IDENT)
            return NULL;
        p->pos++;
        char *name = tok->lexeme;
        size_t arr_size = 0;
        if (match(p, TOK_LBRACKET)) {
            token_t *num = peek(p);
            if (!num || num->type != TOK_NUMBER)
                return NULL;
            p->pos++;
            arr_size = strtoul(num->lexeme, NULL, 10);
            if (!match(p, TOK_RBRACKET))
                return NULL;
            t = TYPE_ARRAY;
        }
        expr_t *init = NULL;
        expr_t **init_list = NULL;
        size_t init_count = 0;
        if (match(p, TOK_ASSIGN)) {
            if (t == TYPE_ARRAY && peek(p) && peek(p)->type == TOK_LBRACE) {
                init_list = parser_parse_init_list(p, &init_count);
                if (!init_list || !match(p, TOK_SEMI)) {
                    if (init_list) {
                        for (size_t i = 0; i < init_count; i++)
                            ast_free_expr(init_list[i]);
                        free(init_list);
                    }
                    return NULL;
                }
            } else {
                init = parser_parse_expr(p);
                if (!init || !match(p, TOK_SEMI)) {
                    ast_free_expr(init);
                    return NULL;
                }
            }
        } else {
            if (!match(p, TOK_SEMI))
                return NULL;
        }
        return ast_make_var_decl(name, t, arr_size, init,
                                 init_list, init_count,
                                 kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_RETURN)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        expr_t *expr = NULL;
        if (!match(p, TOK_SEMI)) {
            expr = parser_parse_expr(p);
            if (!expr || !match(p, TOK_SEMI)) {
                ast_free_expr(expr);
                return NULL;
            }
        }
        return ast_make_return(expr, kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_BREAK)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        if (!match(p, TOK_SEMI))
            return NULL;
        return ast_make_break(kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_CONTINUE)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        if (!match(p, TOK_SEMI))
            return NULL;
        return ast_make_continue(kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_GOTO)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        token_t *id = peek(p);
        if (!id || id->type != TOK_IDENT)
            return NULL;
        p->pos++;
        char *name = id->lexeme;
        if (!match(p, TOK_SEMI))
            return NULL;
        return ast_make_goto(name, kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_IF)) {
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

    if (match(p, TOK_KW_WHILE)) {
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

    if (match(p, TOK_KW_DO)) {
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

    if (match(p, TOK_KW_FOR)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        if (!match(p, TOK_LPAREN))
            return NULL;
        expr_t *init = parser_parse_expr(p);
        if (!init || !match(p, TOK_SEMI)) {
            ast_free_expr(init);
            return NULL;
        }
        expr_t *cond = parser_parse_expr(p);
        if (!cond || !match(p, TOK_SEMI)) {
            ast_free_expr(init);
            ast_free_expr(cond);
            return NULL;
        }
        expr_t *incr = parser_parse_expr(p);
        if (!incr || !match(p, TOK_RPAREN)) {
            ast_free_expr(init);
            ast_free_expr(cond);
            ast_free_expr(incr);
            return NULL;
        }
        stmt_t *body = parser_parse_stmt(p);
        if (!body) {
            ast_free_expr(init);
            ast_free_expr(cond);
            ast_free_expr(incr);
            return NULL;
        }
        return ast_make_for(init, cond, incr, body,
                            kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_SWITCH)) {
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
        vector_t cases_v; vector_init(&cases_v, sizeof(switch_case_t));
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
            if (!stmt) {
                goto error_switch;
            }
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

    expr_t *expr = parser_parse_expr(p);
    if (!expr || !match(p, TOK_SEMI)) {
        ast_free_expr(expr);
        return NULL;
    }
    return ast_make_expr_stmt(expr, expr->line, expr->column);
}
