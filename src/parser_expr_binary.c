/*
 * Binary operator expression parsing helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "parser.h"
#include "parser_types.h"
#include "ast_expr.h"
#include "parser_expr_primary.h"
#include "parser_expr_binary.h"

/* Helper to parse left-associative binary operator chains */
static expr_t *parse_binop_chain(parser_t *p, expr_t *(*sub)(parser_t *),
                                 const token_type_t *tok_list,
                                 const binop_t *op_list, size_t count)
{
    expr_t *left = sub(p);
    if (!left)
        return NULL;

    while (1) {
        size_t idx;
        for (idx = 0; idx < count; idx++) {
            if (match(p, tok_list[idx]))
                break;
        }
        if (idx == count)
            break;

        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *right = sub(p);
        if (!right) {
            ast_free_expr(left);
            return NULL;
        }

        expr_t *tmp = ast_make_binary(op_list[idx], left, right,
                                      op_tok->line, op_tok->column);
        if (!tmp) {
            ast_free_expr(left);
            ast_free_expr(right);
            return NULL;
        }
        left = tmp;
    }

    return left;
}

/* Multiplication and division. */
static expr_t *parse_term(parser_t *p)
{
    static const token_type_t toks[] = { TOK_STAR, TOK_SLASH, TOK_PERCENT };
    static const binop_t ops[] = { BINOP_MUL, BINOP_DIV, BINOP_MOD };
    return parse_binop_chain(p, parse_primary, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Addition and subtraction. */
static expr_t *parse_additive(parser_t *p)
{
    static const token_type_t toks[] = { TOK_PLUS, TOK_MINUS };
    static const binop_t ops[] = { BINOP_ADD, BINOP_SUB };
    return parse_binop_chain(p, parse_term, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Bitwise shifts. */
static expr_t *parse_shift(parser_t *p)
{
    static const token_type_t toks[] = { TOK_SHL, TOK_SHR };
    static const binop_t ops[] = { BINOP_SHL, BINOP_SHR };
    return parse_binop_chain(p, parse_additive, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Comparison operators. */
static expr_t *parse_relational(parser_t *p)
{
    static const token_type_t toks[] = { TOK_LT, TOK_GT, TOK_LE, TOK_GE };
    static const binop_t ops[] = { BINOP_LT, BINOP_GT, BINOP_LE, BINOP_GE };
    return parse_binop_chain(p, parse_shift, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Equality comparison. */
static expr_t *parse_equality(parser_t *p)
{
    static const token_type_t toks[] = { TOK_EQ, TOK_NEQ };
    static const binop_t ops[] = { BINOP_EQ, BINOP_NEQ };
    return parse_binop_chain(p, parse_relational, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Bitwise AND. */
static expr_t *parse_bitand(parser_t *p)
{
    static const token_type_t toks[] = { TOK_AMP };
    static const binop_t ops[] = { BINOP_BITAND };
    return parse_binop_chain(p, parse_equality, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Bitwise XOR. */
static expr_t *parse_bitxor(parser_t *p)
{
    static const token_type_t toks[] = { TOK_CARET };
    static const binop_t ops[] = { BINOP_BITXOR };
    return parse_binop_chain(p, parse_bitand, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Bitwise OR. */
static expr_t *parse_bitor(parser_t *p)
{
    static const token_type_t toks[] = { TOK_PIPE };
    static const binop_t ops[] = { BINOP_BITOR };
    return parse_binop_chain(p, parse_bitxor, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Logical AND. */
static expr_t *parse_logical_and(parser_t *p)
{
    static const token_type_t toks[] = { TOK_LOGAND };
    static const binop_t ops[] = { BINOP_LOGAND };
    return parse_binop_chain(p, parse_bitor, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Logical OR is exported for use by higher-level parsers. */
expr_t *parse_logical_or(parser_t *p)
{
    static const token_type_t toks[] = { TOK_LOGOR };
    static const binop_t ops[] = { BINOP_LOGOR };
    return parse_binop_chain(p, parse_logical_and, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

