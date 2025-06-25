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
#include "parser_types.h"

static stmt_t *parse_block(parser_t *p);
static stmt_t *parse_var_decl(parser_t *p);
stmt_t *parser_parse_enum_decl(parser_t *p);
stmt_t *parser_parse_union_decl(parser_t *p);
stmt_t *parser_parse_union_var_decl(parser_t *p);
stmt_t *parser_parse_stmt(parser_t *p);

/* Parse a "{...}" block recursively collecting inner statements. */
static stmt_t *parse_block(parser_t *p)
{
    if (!match(p, TOK_LBRACE))
        return NULL;
    token_t *lb_tok = &p->tokens[p->pos - 1];
    vector_t stmts_v;
    vector_init(&stmts_v, sizeof(stmt_t *));
    while (!match(p, TOK_RBRACE)) {
        stmt_t *s = parser_parse_stmt(p);
        if (!s) {
            for (size_t i = 0; i < stmts_v.count; i++)
                ast_free_stmt(((stmt_t **)stmts_v.data)[i]);
            vector_free(&stmts_v);
            return NULL;
        }
        if (!vector_push(&stmts_v, &s)) {
            ast_free_stmt(s);
            for (size_t i = 0; i < stmts_v.count; i++)
                ast_free_stmt(((stmt_t **)stmts_v.data)[i]);
            vector_free(&stmts_v);
            return NULL;
        }
    }
    stmt_t **stmts = (stmt_t **)stmts_v.data;
    size_t count = stmts_v.count;
    return ast_make_block(stmts, count, lb_tok->line, lb_tok->column);
}



/* Parse a variable declaration starting after a type keyword already matched */
static stmt_t *parse_var_decl(parser_t *p)
{
    int is_static = match(p, TOK_KW_STATIC);
    int is_const = match(p, TOK_KW_CONST);
    token_t *kw_tok = peek(p);
    type_kind_t t;
    char *tag_name = NULL;
    size_t elem_size = 0;
    if (match(p, TOK_KW_UNION)) {
        token_t *tag_tok = peek(p);
        if (!tag_tok || tag_tok->type != TOK_IDENT)
            return NULL;
        p->pos++;
        tag_name = vc_strdup(tag_tok->lexeme);
        if (!tag_name)
            return NULL;
        t = TYPE_UNION;
    } else {
        if (!parse_basic_type(p, &t))
            return NULL;
        elem_size = basic_type_size(t);
    }
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
    stmt_t *res = ast_make_var_decl(name, t, arr_size, elem_size, is_static,
                                    is_const, init, init_list, init_count,
                                    tag_name, NULL, 0,
                                    kw_tok->line, kw_tok->column);
    if (!res)
        free(tag_name);
    return res;
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

    vector_t items_v;
    vector_init(&items_v, sizeof(enumerator_t));
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
            if (!val) {
                free(name);
                goto fail;
            }
        }
        enumerator_t tmp = { name, val };
        if (!vector_push(&items_v, &tmp)) {
            free(name);
            ast_free_expr(val);
            goto fail;
        }
    } while (match(p, TOK_COMMA));

    if (!match(p, TOK_RBRACE) || !match(p, TOK_SEMI))
        goto fail;

    ok = 1;
fail:
    if (!ok) {
        for (size_t i = 0; i < items_v.count; i++) {
            enumerator_t *it = &((enumerator_t *)items_v.data)[i];
            free(it->name);
            ast_free_expr(it->value);
        }
        free(items_v.data);
        return NULL;
    }
    enumerator_t *items = (enumerator_t *)items_v.data;
    size_t count = items_v.count;
    return ast_make_enum_decl(tag, items, count, kw->line, kw->column);
}

/* Parse a union variable with inline member specification */
stmt_t *parser_parse_union_var_decl(parser_t *p)
{
    int is_static = match(p, TOK_KW_STATIC);
    int is_const = match(p, TOK_KW_CONST);
    if (!match(p, TOK_KW_UNION))
        return NULL;
    token_t *kw = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LBRACE))
        return NULL;

    vector_t members_v;
    vector_init(&members_v, sizeof(union_member_t));
    int ok = 0;
    while (!match(p, TOK_RBRACE)) {
        type_kind_t mt;
        if (!parse_basic_type(p, &mt))
            goto fail;
        size_t elem_size = basic_type_size(mt);
        if (match(p, TOK_STAR))
            mt = TYPE_PTR;
        token_t *id = peek(p);
        if (!id || id->type != TOK_IDENT)
            goto fail;
        p->pos++;
        size_t arr_size = 0;
        if (match(p, TOK_LBRACKET)) {
            token_t *num = peek(p);
            if (!num || num->type != TOK_NUMBER)
                goto fail;
            p->pos++;
            arr_size = strtoul(num->lexeme, NULL, 10);
            if (!match(p, TOK_RBRACKET))
                goto fail;
            mt = TYPE_ARRAY;
        }
        if (!match(p, TOK_SEMI))
            goto fail;
        size_t mem_sz = elem_size;
        if (mt == TYPE_ARRAY)
            mem_sz *= arr_size;
        union_member_t m = { vc_strdup(id->lexeme), mt, mem_sz, 0 };
        if (!vector_push(&members_v, &m)) {
            free(m.name);
            goto fail;
        }
    }

    token_t *name_tok = peek(p);
    if (!name_tok || name_tok->type != TOK_IDENT)
        goto fail;
    p->pos++;
    char *name = name_tok->lexeme;
    if (!match(p, TOK_SEMI))
        goto fail;

    ok = 1;
fail:
    if (!ok) {
        for (size_t i = 0; i < members_v.count; i++)
            free(((union_member_t *)members_v.data)[i].name);
        vector_free(&members_v);
        return NULL;
    }
    union_member_t *members = (union_member_t *)members_v.data;
    size_t count = members_v.count;
    stmt_t *res = ast_make_var_decl(name, TYPE_UNION, 0, 0, is_static, is_const,
                                    NULL, NULL, 0, NULL, members, count,
                                    kw->line, kw->column);
    if (!res) {
        for (size_t i = 0; i < count; i++)
            free(members[i].name);
        free(members);
    }
    return res;
}

/* Parse a named union type declaration */
stmt_t *parser_parse_union_decl(parser_t *p)
{
    if (!match(p, TOK_KW_UNION))
        return NULL;
    token_t *kw = &p->tokens[p->pos - 1];
    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT)
        return NULL;
    p->pos++;
    char *tag = tok->lexeme;
    if (!match(p, TOK_LBRACE))
        return NULL;

    vector_t members_v;
    vector_init(&members_v, sizeof(union_member_t));
    int ok = 0;
    while (!match(p, TOK_RBRACE)) {
        type_kind_t mt;
        if (!parse_basic_type(p, &mt))
            goto fail;
        size_t elem_size = basic_type_size(mt);
        if (match(p, TOK_STAR))
            mt = TYPE_PTR;
        token_t *id = peek(p);
        if (!id || id->type != TOK_IDENT)
            goto fail;
        p->pos++;
        size_t arr_size = 0;
        if (match(p, TOK_LBRACKET)) {
            token_t *num = peek(p);
            if (!num || num->type != TOK_NUMBER)
                goto fail;
            p->pos++;
            arr_size = strtoul(num->lexeme, NULL, 10);
            if (!match(p, TOK_RBRACKET))
                goto fail;
            mt = TYPE_ARRAY;
        }
        if (!match(p, TOK_SEMI))
            goto fail;
        size_t mem_sz = elem_size;
        if (mt == TYPE_ARRAY)
            mem_sz *= arr_size;
        union_member_t m = { vc_strdup(id->lexeme), mt, mem_sz, 0 };
        if (!vector_push(&members_v, &m)) {
            free(m.name);
            goto fail;
        }
    }

    if (!match(p, TOK_SEMI))
        goto fail;

    ok = 1;
fail:
    if (!ok) {
        for (size_t i = 0; i < members_v.count; i++)
            free(((union_member_t *)members_v.data)[i].name);
        vector_free(&members_v);
        return NULL;
    }
    union_member_t *members = (union_member_t *)members_v.data;
    size_t count = members_v.count;
    return ast_make_union_decl(tag, members, count, kw->line, kw->column);
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

    tok = peek(p);
    size_t save = p->pos;
    int has_static = match(p, TOK_KW_STATIC);
    int has_const = match(p, TOK_KW_CONST);
    if (match(p, TOK_KW_UNION)) {
        token_t *next = peek(p);
        if (next && next->type == TOK_LBRACE) {
            p->pos = save;
            return parser_parse_union_var_decl(p);
        } else if (next && next->type == TOK_IDENT) {
            p->pos++;
            token_t *after = peek(p);
            if (!has_static && !has_const && after && after->type == TOK_LBRACE) {
                p->pos = save;
                return parser_parse_union_decl(p);
            }
            p->pos = save;
            return parse_var_decl(p);
        } else {
            p->pos = save;
        }
    } else {
        p->pos = save;
    }
    if (tok && tok->type == TOK_KW_STATIC)
        return parse_var_decl(p);
    if (tok && tok->type == TOK_KW_CONST)
        return parse_var_decl(p);
    if (tok && (tok->type == TOK_KW_INT || tok->type == TOK_KW_CHAR ||
                tok->type == TOK_KW_FLOAT || tok->type == TOK_KW_DOUBLE ||
                tok->type == TOK_KW_SHORT || tok->type == TOK_KW_LONG ||
                tok->type == TOK_KW_BOOL || tok->type == TOK_KW_UNSIGNED)) {
        return parse_var_decl(p);
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
        stmt_t *init_decl = NULL;
        expr_t *init = NULL;
        tok = peek(p);
        if (tok && (tok->type == TOK_KW_STATIC || tok->type == TOK_KW_CONST ||
                    tok->type == TOK_KW_INT || tok->type == TOK_KW_CHAR || tok->type == TOK_KW_FLOAT ||
                    tok->type == TOK_KW_DOUBLE || tok->type == TOK_KW_SHORT ||
                    tok->type == TOK_KW_LONG || tok->type == TOK_KW_BOOL ||
                    tok->type == TOK_KW_UNSIGNED)) {
            init_decl = parse_var_decl(p);
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
