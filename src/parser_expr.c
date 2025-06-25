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
static expr_t *parse_shift(parser_t *p);
static expr_t *parse_bitand(parser_t *p);
static expr_t *parse_bitxor(parser_t *p);
static expr_t *parse_bitor(parser_t *p);
static expr_t *parse_primary(parser_t *p);
static int parse_type(parser_t *p, type_kind_t *out_type, size_t *out_size);
static expr_t *parse_logical_and(parser_t *p);
static expr_t *parse_logical_or(parser_t *p);

/* Recursively clone an expression tree. Returns NULL on allocation failure. */
static expr_t *clone_expr(const expr_t *expr)
{
    if (!expr)
        return NULL;
    switch (expr->kind) {
    case EXPR_NUMBER:
        return ast_make_number(expr->number.value, expr->line, expr->column);
    case EXPR_IDENT:
        return ast_make_ident(expr->ident.name, expr->line, expr->column);
    case EXPR_STRING:
        return ast_make_string(expr->string.value, expr->line, expr->column);
    case EXPR_CHAR:
        return ast_make_char(expr->ch.value, expr->line, expr->column);
    case EXPR_UNARY: {
        expr_t *op = clone_expr(expr->unary.operand);
        if (!op)
            return NULL;
        return ast_make_unary(expr->unary.op, op, expr->line, expr->column);
    }
    case EXPR_BINARY: {
        expr_t *l = clone_expr(expr->binary.left);
        expr_t *r = clone_expr(expr->binary.right);
        if (!l || !r) {
            ast_free_expr(l);
            ast_free_expr(r);
            return NULL;
        }
        return ast_make_binary(expr->binary.op, l, r,
                               expr->line, expr->column);
    }
    case EXPR_ASSIGN: {
        expr_t *v = clone_expr(expr->assign.value);
        if (!v)
            return NULL;
        return ast_make_assign(expr->assign.name, v,
                               expr->line, expr->column);
    }
    case EXPR_INDEX: {
        expr_t *a = clone_expr(expr->index.array);
        expr_t *i = clone_expr(expr->index.index);
        if (!a || !i) {
            ast_free_expr(a);
            ast_free_expr(i);
            return NULL;
        }
        return ast_make_index(a, i, expr->line, expr->column);
    }
    case EXPR_ASSIGN_INDEX: {
        expr_t *a = clone_expr(expr->assign_index.array);
        expr_t *i = clone_expr(expr->assign_index.index);
        expr_t *v = clone_expr(expr->assign_index.value);
        if (!a || !i || !v) {
            ast_free_expr(a);
            ast_free_expr(i);
            ast_free_expr(v);
            return NULL;
        }
        return ast_make_assign_index(a, i, v,
                                     expr->line, expr->column);
    }
    case EXPR_MEMBER: {
        expr_t *obj = clone_expr(expr->member.object);
        if (!obj)
            return NULL;
        return ast_make_member(obj, expr->member.member,
                               expr->member.via_ptr,
                               expr->line, expr->column);
    }
    case EXPR_SIZEOF:
        if (expr->sizeof_expr.is_type)
            return ast_make_sizeof_type(expr->sizeof_expr.type,
                                        expr->sizeof_expr.array_size,
                                        expr->line, expr->column);
        else {
            expr_t *e = clone_expr(expr->sizeof_expr.expr);
            if (!e)
                return NULL;
            return ast_make_sizeof_expr(e, expr->line, expr->column);
        }
    case EXPR_CALL: {
        size_t n = expr->call.arg_count;
        expr_t **args = NULL;
        if (n) {
            args = malloc(n * sizeof(*args));
            if (!args)
                return NULL;
            for (size_t i = 0; i < n; i++) {
                args[i] = clone_expr(expr->call.args[i]);
                if (!args[i]) {
                    for (size_t j = 0; j < i; j++)
                        ast_free_expr(args[j]);
                    free(args);
                    return NULL;
                }
            }
        }
        return ast_make_call(expr->call.name, args, n,
                             expr->line, expr->column);
    }
    }
    return NULL;
}

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

    if (match(p, TOK_INC)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *op = parse_primary(p);
        if (!op)
            return NULL;
        return ast_make_unary(UNOP_PREINC, op, op_tok->line, op_tok->column);
    } else if (match(p, TOK_DEC)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *op = parse_primary(p);
        if (!op)
            return NULL;
        return ast_make_unary(UNOP_PREDEC, op, op_tok->line, op_tok->column);
    } else if (match(p, TOK_STAR)) {
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
    } else if (match(p, TOK_NOT)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *op = parse_primary(p);
        if (!op)
            return NULL;
        return ast_make_unary(UNOP_NOT, op, op_tok->line, op_tok->column);
    } else if (match(p, TOK_KW_SIZEOF)) {
        token_t *kw = &p->tokens[p->pos - 1];
        if (!match(p, TOK_LPAREN))
            return NULL;
        size_t save = p->pos;
        type_kind_t t; size_t sz;
        if (parse_type(p, &t, &sz) && match(p, TOK_RPAREN))
            return ast_make_sizeof_type(t, sz, kw->line, kw->column);
        p->pos = save;
        expr_t *e = parse_expression(p);
        if (!e || !match(p, TOK_RPAREN)) {
            ast_free_expr(e);
            return NULL;
        }
        return ast_make_sizeof_expr(e, kw->line, kw->column);
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
    while (1) {
        if (match(p, TOK_LBRACKET)) {
            token_t *lb = &p->tokens[p->pos - 1];
            expr_t *idx = parse_expression(p);
            if (!idx || !match(p, TOK_RBRACKET)) {
                ast_free_expr(base);
                ast_free_expr(idx);
                return NULL;
            }
            base = ast_make_index(base, idx, lb->line, lb->column);
        } else if (match(p, TOK_DOT) || match(p, TOK_ARROW)) {
            int via_ptr = (p->tokens[p->pos - 1].type == TOK_ARROW);
            token_t *id = peek(p);
            if (!id || id->type != TOK_IDENT) {
                ast_free_expr(base);
                return NULL;
            }
            p->pos++;
            base = ast_make_member(base, id->lexeme, via_ptr,
                                   id->line, id->column);
        } else if (match(p, TOK_INC)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            base = ast_make_unary(UNOP_POSTINC, base,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_DEC)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            base = ast_make_unary(UNOP_POSTDEC, base,
                                  op_tok->line, op_tok->column);
        } else {
            break;
        }
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
        } else if (match(p, TOK_PERCENT)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_primary(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_MOD, left, right,
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
static expr_t *parse_shift(parser_t *p)
{
    expr_t *left = parse_additive(p);
    if (!left)
        return NULL;

    while (1) {
        if (match(p, TOK_SHL)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_additive(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_SHL, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_SHR)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_additive(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_SHR, left, right,
                                  op_tok->line, op_tok->column);
        } else {
            break;
        }
    }
    return left;
}

static expr_t *parse_relational(parser_t *p)
{
    expr_t *left = parse_shift(p);
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

static expr_t *parse_bitand(parser_t *p)
{
    expr_t *left = parse_equality(p);
    if (!left)
        return NULL;

    while (match(p, TOK_AMP)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *right = parse_equality(p);
        if (!right) {
            ast_free_expr(left);
            return NULL;
        }
        left = ast_make_binary(BINOP_BITAND, left, right,
                              op_tok->line, op_tok->column);
    }
    return left;
}

static expr_t *parse_bitxor(parser_t *p)
{
    expr_t *left = parse_bitand(p);
    if (!left)
        return NULL;

    while (match(p, TOK_CARET)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *right = parse_bitand(p);
        if (!right) {
            ast_free_expr(left);
            return NULL;
        }
        left = ast_make_binary(BINOP_BITXOR, left, right,
                              op_tok->line, op_tok->column);
    }
    return left;
}

static expr_t *parse_bitor(parser_t *p)
{
    expr_t *left = parse_bitxor(p);
    if (!left)
        return NULL;

    while (match(p, TOK_PIPE)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *right = parse_bitxor(p);
        if (!right) {
            ast_free_expr(left);
            return NULL;
        }
        left = ast_make_binary(BINOP_BITOR, left, right,
                              op_tok->line, op_tok->column);
    }
    return left;
}

/* Parse logical AND expressions. */
static expr_t *parse_logical_and(parser_t *p)
{
    expr_t *left = parse_bitor(p);
    if (!left)
        return NULL;

    while (match(p, TOK_LOGAND)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *right = parse_bitor(p);
        if (!right) {
            ast_free_expr(left);
            return NULL;
        }
        left = ast_make_binary(BINOP_LOGAND, left, right,
                              op_tok->line, op_tok->column);
    }
    return left;
}

/* Parse logical OR expressions. */
static expr_t *parse_logical_or(parser_t *p)
{
    expr_t *left = parse_logical_and(p);
    if (!left)
        return NULL;

    while (match(p, TOK_LOGOR)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *right = parse_logical_and(p);
        if (!right) {
            ast_free_expr(left);
            return NULL;
        }
        left = ast_make_binary(BINOP_LOGOR, left, right,
                              op_tok->line, op_tok->column);
    }
    return left;
}

/* Assignment has the lowest precedence and recurses to itself for chained
 * assignments. */
static expr_t *parse_assignment(parser_t *p)
{
    expr_t *left = parse_logical_or(p);
    if (!left)
        return NULL;

    if (match(p, TOK_ASSIGN) || match(p, TOK_PLUSEQ) || match(p, TOK_MINUSEQ) ||
        match(p, TOK_STAREQ) || match(p, TOK_SLASHEQ) ||
        match(p, TOK_PERCENTEQ) || match(p, TOK_AMPEQ) ||
        match(p, TOK_PIPEEQ) || match(p, TOK_CARETEQ) ||
        match(p, TOK_SHLEQ) || match(p, TOK_SHREQ)) {
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
        binop_t bop = BINOP_ADD;
        int compound = 1;
        switch (op_tok->type) {
        case TOK_ASSIGN: compound = 0; break;
        case TOK_PLUSEQ: bop = BINOP_ADD; break;
        case TOK_MINUSEQ: bop = BINOP_SUB; break;
        case TOK_STAREQ: bop = BINOP_MUL; break;
        case TOK_SLASHEQ: bop = BINOP_DIV; break;
        case TOK_PERCENTEQ: bop = BINOP_MOD; break;
        case TOK_AMPEQ: bop = BINOP_BITAND; break;
        case TOK_PIPEEQ: bop = BINOP_BITOR; break;
        case TOK_CARETEQ: bop = BINOP_BITXOR; break;
        case TOK_SHLEQ: bop = BINOP_SHL; break;
        case TOK_SHREQ: bop = BINOP_SHR; break;
        default: compound = 0; break;
        }
        if (compound) {
            expr_t *lhs_copy = clone_expr(left);
            if (!lhs_copy) {
                ast_free_expr(left);
                ast_free_expr(right);
                return NULL;
            }
            right = ast_make_binary(bop, lhs_copy, right,
                                   op_tok->line, op_tok->column);
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

/* Parse a basic type specification used by sizeof. */
static int parse_type(parser_t *p, type_kind_t *out_type, size_t *out_size)
{
    size_t save = p->pos;
    type_kind_t t;
    if (match(p, TOK_KW_INT)) {
        t = TYPE_INT;
    } else if (match(p, TOK_KW_CHAR)) {
        t = TYPE_CHAR;
    } else if (match(p, TOK_KW_FLOAT)) {
        t = TYPE_FLOAT;
    } else if (match(p, TOK_KW_DOUBLE)) {
        t = TYPE_DOUBLE;
    } else if (match(p, TOK_KW_VOID)) {
        t = TYPE_VOID;
    } else {
        p->pos = save;
        return 0;
    }
    if ((t == TYPE_INT || t == TYPE_CHAR || t == TYPE_FLOAT || t == TYPE_DOUBLE) && match(p, TOK_STAR))
        t = TYPE_PTR;
    size_t arr = 0;
    if (match(p, TOK_LBRACKET)) {
        token_t *num = peek(p);
        if (!num || num->type != TOK_NUMBER) {
            p->pos = save;
            return 0;
        }
        p->pos++;
        arr = strtoul(num->lexeme, NULL, 10);
        if (!match(p, TOK_RBRACKET)) {
            p->pos = save;
            return 0;
        }
        t = TYPE_ARRAY;
    }
    if (out_type)
        *out_type = t;
    if (out_size)
        *out_size = arr;
    return 1;
}

