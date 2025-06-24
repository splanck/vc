#include <stdlib.h>
#include "parser.h"

static token_t *peek(parser_t *p)
{
    if (p->pos >= p->count)
        return NULL;
    return &p->tokens[p->pos];
}

static int match(parser_t *p, token_type_t type)
{
    token_t *tok = peek(p);
    if (tok && tok->type == type) {
        p->pos++;
        return 1;
    }
    return 0;
}

void parser_init(parser_t *p, token_t *tokens, size_t count)
{
    p->tokens = tokens;
    p->count = count;
    p->pos = 0;
}

int parser_is_eof(parser_t *p)
{
    token_t *tok = peek(p);
    return !tok || tok->type == TOK_EOF;
}

/* Forward declarations */
static expr_t *parse_expression(parser_t *p);
static expr_t *parse_assignment(parser_t *p);
static expr_t *parse_additive(parser_t *p);
static expr_t *parse_primary(parser_t *p)
{
    token_t *tok = peek(p);
    if (!tok)
        return NULL;

    if (match(p, TOK_STAR)) {
        expr_t *op = parse_primary(p);
        if (!op)
            return NULL;
        return ast_make_unary(UNOP_DEREF, op);
    } else if (match(p, TOK_AMP)) {
        expr_t *op = parse_primary(p);
        if (!op)
            return NULL;
        return ast_make_unary(UNOP_ADDR, op);
    }

    if (match(p, TOK_NUMBER)) {
        return ast_make_number(tok->lexeme);
    } else if (match(p, TOK_STRING)) {
        return ast_make_string(tok->lexeme);
    } else if (match(p, TOK_CHAR)) {
        return ast_make_char(tok->lexeme[0]);
    } else if (tok->type == TOK_IDENT) {
        token_t *next = p->pos + 1 < p->count ? &p->tokens[p->pos + 1] : NULL;
        if (next && next->type == TOK_LPAREN) {
            p->pos++; /* consume ident */
            char *name = tok->lexeme;
            match(p, TOK_LPAREN);
            if (!match(p, TOK_RPAREN))
                return NULL;
            return ast_make_call(name);
        } else if (match(p, TOK_IDENT)) {
            return ast_make_ident(tok->lexeme);
        }
    } else if (match(p, TOK_LPAREN)) {
        expr_t *expr = parse_expression(p);
        if (!match(p, TOK_RPAREN)) {
            ast_free_expr(expr);
            return NULL;
        }
        return expr;
    }
    return NULL;
}

static expr_t *parse_term(parser_t *p)
{
    expr_t *left = parse_primary(p);
    if (!left)
        return NULL;

    while (1) {
        if (match(p, TOK_STAR)) {
            expr_t *right = parse_primary(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_MUL, left, right);
        } else if (match(p, TOK_SLASH)) {
            expr_t *right = parse_primary(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_DIV, left, right);
        } else {
            break;
        }
    }
    return left;
}

static expr_t *parse_additive(parser_t *p)
{
    expr_t *left = parse_term(p);
    if (!left)
        return NULL;

    while (1) {
        if (match(p, TOK_PLUS)) {
            expr_t *right = parse_term(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_ADD, left, right);
        } else if (match(p, TOK_MINUS)) {
            expr_t *right = parse_term(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_SUB, left, right);
        } else {
            break;
        }
    }
    return left;
}

static expr_t *parse_assignment(parser_t *p)
{
    expr_t *left = parse_additive(p);
    if (!left)
        return NULL;

    if (match(p, TOK_ASSIGN)) {
        if (left->kind != EXPR_IDENT) {
            ast_free_expr(left);
            return NULL;
        }
        expr_t *right = parse_assignment(p);
        if (!right) {
            free(left->ident.name);
            free(left);
            return NULL;
        }
        char *name = left->ident.name;
        free(left);
        left = ast_make_assign(name, right);
        free(name);
    }
    return left;
}

static expr_t *parse_expression(parser_t *p)
{
    return parse_assignment(p);
}

expr_t *parser_parse_expr(parser_t *p)
{
    return parse_expression(p);
}

stmt_t *parser_parse_stmt(parser_t *p)
{
    if (match(p, TOK_KW_INT)) {
        type_kind_t t = TYPE_INT;
        if (match(p, TOK_STAR))
            t = TYPE_PTR;
        token_t *tok = peek(p);
        if (!tok || tok->type != TOK_IDENT)
            return NULL;
        p->pos++;
        char *name = tok->lexeme;
        if (!match(p, TOK_SEMI))
            return NULL;
        return ast_make_var_decl(name, t);
    }

    if (match(p, TOK_KW_RETURN)) {
        expr_t *expr = parse_expression(p);
        if (!expr || !match(p, TOK_SEMI)) {
            ast_free_expr(expr);
            return NULL;
        }
        return ast_make_return(expr);
    }

    if (match(p, TOK_KW_IF)) {
        if (!match(p, TOK_LPAREN))
            return NULL;
        expr_t *cond = parse_expression(p);
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
        return ast_make_if(cond, then_branch, else_branch);
    }

    if (match(p, TOK_KW_WHILE)) {
        if (!match(p, TOK_LPAREN))
            return NULL;
        expr_t *cond = parse_expression(p);
        if (!cond || !match(p, TOK_RPAREN)) {
            ast_free_expr(cond);
            return NULL;
        }
        stmt_t *body = parser_parse_stmt(p);
        if (!body) {
            ast_free_expr(cond);
            return NULL;
        }
        return ast_make_while(cond, body);
    }

    expr_t *expr = parse_expression(p);
    if (!expr || !match(p, TOK_SEMI)) {
        ast_free_expr(expr);
        return NULL;
    }
    return ast_make_expr_stmt(expr);
}

func_t *parser_parse_func(parser_t *p)
{
    type_kind_t ret_type;
    if (match(p, TOK_KW_INT)) {
        ret_type = TYPE_INT;
    } else if (match(p, TOK_KW_VOID)) {
        ret_type = TYPE_VOID;
    } else {
        return NULL;
    }

    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT)
        return NULL;
    p->pos++;
    char *name = tok->lexeme;

    if (!match(p, TOK_LPAREN) || !match(p, TOK_RPAREN))
        return NULL;
    if (!match(p, TOK_LBRACE))
        return NULL;

    size_t cap = 4, count = 0;
    stmt_t **body = malloc(cap * sizeof(*body));
    if (!body)
        return NULL;

    while (!match(p, TOK_RBRACE)) {
        stmt_t *stmt = parser_parse_stmt(p);
        if (!stmt) {
            for (size_t i = 0; i < count; i++)
                ast_free_stmt(body[i]);
            free(body);
            return NULL;
        }
        if (count >= cap) {
            cap *= 2;
            stmt_t **tmp = realloc(body, cap * sizeof(*tmp));
            if (!tmp) {
                for (size_t i = 0; i < count; i++)
                    ast_free_stmt(body[i]);
                free(body);
                return NULL;
            }
            body = tmp;
        }
        body[count++] = stmt;
    }

    return ast_make_func(name, ret_type, body, count);
}
