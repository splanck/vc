/*
 * Recursive descent expression parser.
 *
 * Expressions are parsed starting from the lowest precedence
 * (assignments) down to primary terms.  Each helper returns a newly
 * allocated expr_t and advances the parser on success.  A NULL return
 * indicates a syntax error.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "parser.h"

/* Forward declarations */
static expr_t *parse_expression(parser_t *p);
static expr_t *parse_assignment(parser_t *p);
static expr_t *parse_equality(parser_t *p);
static expr_t *parse_relational(parser_t *p);
static expr_t *parse_additive(parser_t *p);
static expr_t *parse_primary(parser_t *p);

/*
 * Parse the most basic expression forms: literals, identifiers, function
 * calls and array indexing.  Prefix unary operators are also handled
 * here.  The returned expr_t represents the parsed sub-expression.
 */
static expr_t *parse_primary(parser_t *p)
{
    token_t *tok = peek(p);
    if (!tok)
        return NULL;

    if (match(p, TOK_STAR)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *op = parse_primary(p);
        if (!op)
            return NULL;
        return ast_make_unary(UNOP_DEREF, op, op_tok->line, op_tok->column);
    } else if (match(p, TOK_AMP)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *op = parse_primary(p);
        if (!op)
            return NULL;
        return ast_make_unary(UNOP_ADDR, op, op_tok->line, op_tok->column);
    } else if (match(p, TOK_MINUS)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *op = parse_primary(p);
        if (!op)
            return NULL;
        return ast_make_unary(UNOP_NEG, op, op_tok->line, op_tok->column);
    }

    expr_t *base = NULL;
    if (match(p, TOK_NUMBER)) {
        base = ast_make_number(tok->lexeme, tok->line, tok->column);
    } else if (match(p, TOK_STRING)) {
        base = ast_make_string(tok->lexeme, tok->line, tok->column);
    } else if (match(p, TOK_CHAR)) {
        base = ast_make_char(tok->lexeme[0], tok->line, tok->column);
    } else if (tok->type == TOK_IDENT) {
        token_t *next = p->pos + 1 < p->count ? &p->tokens[p->pos + 1] : NULL;
        if (next && next->type == TOK_LPAREN) {
            p->pos++; /* consume ident */
            char *name = tok->lexeme;
            match(p, TOK_LPAREN);
            size_t cap = 4, count = 0;
            expr_t **args = malloc(cap * sizeof(*args));
            if (!args)
                return NULL;
            if (!match(p, TOK_RPAREN)) {
                do {
                    expr_t *arg = parse_expression(p);
                    if (!arg) {
                        for (size_t i = 0; i < count; i++)
                            ast_free_expr(args[i]);
                        free(args);
                        return NULL;
                    }
                    if (count >= cap) {
                        cap *= 2;
                        expr_t **tmp = realloc(args, cap * sizeof(*tmp));
                        if (!tmp) {
                            for (size_t i = 0; i < count; i++)
                                ast_free_expr(args[i]);
                            free(args);
                            return NULL;
                        }
                        args = tmp;
                    }
                    args[count++] = arg;
                } while (match(p, TOK_COMMA));
                if (!match(p, TOK_RPAREN)) {
                    for (size_t i = 0; i < count; i++)
                        ast_free_expr(args[i]);
                    free(args);
                    return NULL;
                }
            }
            expr_t *call = ast_make_call(name, args, count,
                                         tok->line, tok->column);
            base = call;
        } else if (match(p, TOK_IDENT)) {
            base = ast_make_ident(tok->lexeme, tok->line, tok->column);
        }
    } else if (match(p, TOK_LPAREN)) {
        expr_t *expr = parse_expression(p);
        if (!match(p, TOK_RPAREN)) {
            ast_free_expr(expr);
            return NULL;
        }
        base = expr;
    }
    if (!base)
        return NULL;
    while (match(p, TOK_LBRACKET)) {
        token_t *lb = &p->tokens[p->pos - 1];
        expr_t *idx = parse_expression(p);
        if (!idx || !match(p, TOK_RBRACKET)) {
            ast_free_expr(base);
            ast_free_expr(idx);
            return NULL;
        }
        base = ast_make_index(base, idx, lb->line, lb->column);
    }
    return base;
}

/*
 * Handle multiplication and division.  The function expects that any
 * higher precedence unary/primary expression has already been consumed
 * and returns the combined binary expression tree.
 */
static expr_t *parse_term(parser_t *p)
{
    expr_t *left = parse_primary(p);
    if (!left)
        return NULL;

    while (1) {
        if (match(p, TOK_STAR)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_primary(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_MUL, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_SLASH)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_primary(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_DIV, left, right,
                                  op_tok->line, op_tok->column);
        } else {
            break;
        }
    }
    return left;
}

/* Build addition and subtraction expressions. */
static expr_t *parse_additive(parser_t *p)
{
    expr_t *left = parse_term(p);
    if (!left)
        return NULL;

    while (1) {
        if (match(p, TOK_PLUS)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_term(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_ADD, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_MINUS)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_term(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_SUB, left, right,
                                  op_tok->line, op_tok->column);
        } else {
            break;
        }
    }
    return left;
}

/* Comparison operators <, >, <= and >=. */
static expr_t *parse_relational(parser_t *p)
{
    expr_t *left = parse_additive(p);
    if (!left)
        return NULL;

    while (1) {
        if (match(p, TOK_LT)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_additive(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_LT, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_GT)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_additive(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_GT, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_LE)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_additive(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_LE, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_GE)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_additive(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_GE, left, right,
                                  op_tok->line, op_tok->column);
        } else {
            break;
        }
    }
    return left;
}

/* Parse == and != comparisons. */
static expr_t *parse_equality(parser_t *p)
{
    expr_t *left = parse_relational(p);
    if (!left)
        return NULL;

    while (1) {
        if (match(p, TOK_EQ)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_relational(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_EQ, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_NEQ)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_relational(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_NEQ, left, right,
                                  op_tok->line, op_tok->column);
        } else {
            break;
        }
    }
    return left;
}

/* Assignment has the lowest precedence and recurses to itself for chained
 * assignments. */
static expr_t *parse_assignment(parser_t *p)
{
    expr_t *left = parse_equality(p);
    if (!left)
        return NULL;

    if (match(p, TOK_ASSIGN)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        if (left->kind != EXPR_IDENT && left->kind != EXPR_INDEX) {
            ast_free_expr(left);
            return NULL;
        }
        expr_t *right = parse_assignment(p);
        if (!right) {
            if (left->kind == EXPR_IDENT)
                free(left->ident.name);
            ast_free_expr(left);
            return NULL;
        }
        if (left->kind == EXPR_IDENT) {
            char *name = left->ident.name;
            free(left);
            left = ast_make_assign(name, right, op_tok->line, op_tok->column);
            free(name);
        } else {
            expr_t *arr = left->index.array;
            expr_t *idx = left->index.index;
            free(left);
            left = ast_make_assign_index(arr, idx, right,
                                        op_tok->line, op_tok->column);
        }
    }
    return left;
}

/* Entry point that parses the full expression grammar. */
static expr_t *parse_expression(parser_t *p)
{
    return parse_assignment(p);
}

/*
 * Parse an initializer list surrounded by braces.  The returned array of
 * expressions has "*out_count" elements and must be freed by the
 * caller.  NULL is returned on error.
 */
expr_t **parser_parse_init_list(parser_t *p, size_t *out_count)
{
    if (!match(p, TOK_LBRACE))
        return NULL;
    size_t cap = 4, count = 0;
    expr_t **vals = malloc(cap * sizeof(*vals));
    if (!vals)
        return NULL;
    if (!match(p, TOK_RBRACE)) {
        do {
            expr_t *e = parse_expression(p);
            if (!e) {
                for (size_t i = 0; i < count; i++)
                    ast_free_expr(vals[i]);
                free(vals);
                return NULL;
            }
            if (count >= cap) {
                cap *= 2;
                expr_t **tmp = realloc(vals, cap * sizeof(*tmp));
                if (!tmp) {
                    for (size_t i = 0; i < count; i++)
                        ast_free_expr(vals[i]);
                    free(vals);
                    return NULL;
                }
                vals = tmp;
            }
            vals[count++] = e;
        } while (match(p, TOK_COMMA));
        if (!match(p, TOK_RBRACE)) {
            for (size_t i = 0; i < count; i++)
                ast_free_expr(vals[i]);
            free(vals);
            return NULL;
        }
    }
    if (out_count)
        *out_count = count;
    return vals;
}

/* Public wrapper for expression parsing used by other modules. */
expr_t *parser_parse_expr(parser_t *p)
{
    return parse_expression(p);
}

