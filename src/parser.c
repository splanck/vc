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

    if (match(p, TOK_NUMBER)) {
        return ast_make_number(tok->lexeme);
    } else if (match(p, TOK_IDENT)) {
        return ast_make_ident(tok->lexeme);
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
        token_t *tok = peek(p);
        if (!tok || tok->type != TOK_IDENT)
            return NULL;
        p->pos++;
        char *name = tok->lexeme;
        if (!match(p, TOK_SEMI))
            return NULL;
        return ast_make_var_decl(name);
    }

    if (match(p, TOK_KW_RETURN)) {
        expr_t *expr = parse_expression(p);
        if (!expr || !match(p, TOK_SEMI)) {
            ast_free_expr(expr);
            return NULL;
        }
        return ast_make_return(expr);
    }

    expr_t *expr = parse_expression(p);
    if (!expr || !match(p, TOK_SEMI)) {
        ast_free_expr(expr);
        return NULL;
    }
    return ast_make_expr_stmt(expr);
}
