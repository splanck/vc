#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "parser_types.h"
#include "ast_expr.h"
#include "util.h"
#include "error.h"
#include "parser_expr_primary.h"
#include "parser_expr_ops.h"

/* --- Postfix operators --- */

static expr_t *parse_index_op(parser_t *p, expr_t *base)
{
    if (!match(p, TOK_LBRACKET))
        return base;

    token_t *lb = &p->tokens[p->pos - 1];
    expr_t *idx = parser_parse_expr(p);
    if (!idx || !match(p, TOK_RBRACKET)) {
        ast_free_expr(base);
        ast_free_expr(idx);
        return NULL;
    }

    return ast_make_index(base, idx, lb->line, lb->column);
}

static expr_t *parse_member_op(parser_t *p, expr_t *base)
{
    if (!match(p, TOK_DOT) && !match(p, TOK_ARROW))
        return base;

    int via_ptr = (p->tokens[p->pos - 1].type == TOK_ARROW);
    token_t *id = peek(p);
    if (!id || id->type != TOK_IDENT) {
        ast_free_expr(base);
        return NULL;
    }
    p->pos++;
    return ast_make_member(base, id->lexeme, via_ptr, id->line, id->column);
}

static expr_t *parse_postincdec(parser_t *p, expr_t *base)
{
    if (match(p, TOK_INC)) {
        token_t *tok = &p->tokens[p->pos - 1];
        return ast_make_unary(UNOP_POSTINC, base, tok->line, tok->column);
    }
    if (match(p, TOK_DEC)) {
        token_t *tok = &p->tokens[p->pos - 1];
        return ast_make_unary(UNOP_POSTDEC, base, tok->line, tok->column);
    }
    return base;
}

static expr_t *apply_postfix_ops(parser_t *p, expr_t *base)
{
    while (1) {
        expr_t *next = base;

        next = parse_index_op(p, next);
        if (!next)
            return NULL;
        if (next != base) {
            base = next;
            continue;
        }

        next = parse_member_op(p, next);
        if (!next)
            return NULL;
        if (next != base) {
            base = next;
            continue;
        }

        next = parse_postincdec(p, next);
        if (!next)
            return NULL;
        if (next != base) {
            base = next;
            continue;
        }

        break;
    }
    return base;
}

expr_t *parse_postfix_expr(parser_t *p)
{
    expr_t *base = parse_base_term(p);
    if (!base)
        return NULL;
    return apply_postfix_ops(p, base);
}

/* --- Prefix operators --- */

static expr_t *parse_preinc(parser_t *p)
{
    token_t *op_tok = &p->tokens[p->pos - 1];
    expr_t *op = parse_prefix_expr(p);
    if (!op)
        return NULL;
    return ast_make_unary(UNOP_PREINC, op, op_tok->line, op_tok->column);
}

static expr_t *parse_predec(parser_t *p)
{
    token_t *op_tok = &p->tokens[p->pos - 1];
    expr_t *op = parse_prefix_expr(p);
    if (!op)
        return NULL;
    return ast_make_unary(UNOP_PREDEC, op, op_tok->line, op_tok->column);
}

static expr_t *parse_deref(parser_t *p)
{
    token_t *op_tok = &p->tokens[p->pos - 1];
    expr_t *op = parse_prefix_expr(p);
    if (!op)
        return NULL;
    return ast_make_unary(UNOP_DEREF, op, op_tok->line, op_tok->column);
}

static expr_t *parse_addr(parser_t *p)
{
    token_t *op_tok = &p->tokens[p->pos - 1];
    expr_t *op = parse_prefix_expr(p);
    if (!op)
        return NULL;
    return ast_make_unary(UNOP_ADDR, op, op_tok->line, op_tok->column);
}

static expr_t *parse_neg(parser_t *p)
{
    token_t *op_tok = &p->tokens[p->pos - 1];
    expr_t *op = parse_prefix_expr(p);
    if (!op)
        return NULL;
    return ast_make_unary(UNOP_NEG, op, op_tok->line, op_tok->column);
}

static expr_t *parse_not(parser_t *p)
{
    token_t *op_tok = &p->tokens[p->pos - 1];
    expr_t *op = parse_prefix_expr(p);
    if (!op)
        return NULL;
    return ast_make_unary(UNOP_NOT, op, op_tok->line, op_tok->column);
}

static expr_t *parse_sizeof(parser_t *p)
{
    token_t *kw = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LPAREN))
        return NULL;
    size_t save = p->pos;
    type_kind_t t; size_t sz; size_t esz;
    if (parse_type(p, &t, &sz, &esz) && match(p, TOK_RPAREN))
        return ast_make_sizeof_type(t, sz, esz, kw->line, kw->column);
    p->pos = save;
    expr_t *e = parser_parse_expr(p);
    if (!e || !match(p, TOK_RPAREN)) {
        ast_free_expr(e);
        return NULL;
    }
    return ast_make_sizeof_expr(e, kw->line, kw->column);
}

static expr_t *parse_alignof(parser_t *p)
{
    token_t *kw = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LPAREN))
        return NULL;
    size_t save = p->pos;
    type_kind_t t; size_t sz; size_t esz;
    if (parse_type(p, &t, &sz, &esz) && match(p, TOK_RPAREN))
        return ast_make_alignof_type(t, sz, esz, kw->line, kw->column);
    p->pos = save;
    expr_t *e = parser_parse_expr(p);
    if (!e || !match(p, TOK_RPAREN)) {
        ast_free_expr(e);
        return NULL;
    }
    return ast_make_alignof_expr(e, kw->line, kw->column);
}

static expr_t *parse_prefix_operator(parser_t *p)
{
    static const struct {
        token_type_t tok;
        expr_t *(*fn)(parser_t *);
    } table[] = {
        { TOK_INC,       parse_preinc },
        { TOK_DEC,       parse_predec },
        { TOK_STAR,      parse_deref },
        { TOK_AMP,       parse_addr },
        { TOK_MINUS,     parse_neg },
        { TOK_NOT,       parse_not },
        { TOK_KW_SIZEOF, parse_sizeof },
        { TOK_KW_ALIGNOF, parse_alignof }
    };

    token_t *tok = peek(p);
    if (!tok)
        return NULL;

    for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); i++) {
        if (match(p, table[i].tok))
            return table[i].fn(p);
    }

    return NULL;
}

int parse_type(parser_t *p, type_kind_t *out_type, size_t *out_size,
               size_t *elem_size)
{
    size_t save = p->pos;
    type_kind_t t;
    if (!parse_basic_type(p, &t))
        return 0;
    size_t esz = basic_type_size(t);
    if (match(p, TOK_STAR))
        t = TYPE_PTR;
    size_t arr = 0;
    if (match(p, TOK_LBRACKET)) {
        token_t *num = peek(p);
        if (!num || num->type != TOK_NUMBER) {
            p->pos = save;
            return 0;
        }
        p->pos++;
        if (!vc_strtoul_size(num->lexeme, &arr)) {
            error_set(&error_ctx, num->line, num->column, NULL, NULL);
            error_print(&error_ctx, "Integer constant out of range");
            p->pos = save;
            return 0;
        }
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
    if (elem_size)
        *elem_size = esz;
    return 1;
}

expr_t *parse_cast(parser_t *p)
{
    size_t save = p->pos;
    if (!match(p, TOK_LPAREN))
        return NULL;

    token_t *lp = &p->tokens[p->pos - 1];
    type_kind_t t; size_t sz; size_t esz;

    if (!parse_type(p, &t, &sz, &esz) || !match(p, TOK_RPAREN)) {
        p->pos = save;
        return NULL;
    }

    expr_t *inner = parse_cast(p);
    if (!inner)
        inner = parse_prefix_expr(p);
    if (!inner) {
        p->pos = save;
        return NULL;
    }

    expr_t *res = ast_make_cast(t, sz, esz, inner, lp->line, lp->column);
    if (!res) {
        ast_free_expr(inner);
        p->pos = save;
    }
    return res;
}

expr_t *parse_prefix_expr(parser_t *p)
{
    expr_t *op = parse_prefix_operator(p);
    if (op)
        return op;

    return parse_postfix_expr(p);
}

