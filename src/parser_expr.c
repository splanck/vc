/*
 * Recursive descent expression parser.
 *
 * Expressions are parsed starting from the lowest precedence
 * (assignments) down to primary terms.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "parser_types.h"
#include "ast_expr.h"
#include "util.h"
#include "ast_clone.h"
#include "error.h"
#include "parser_expr_primary.h"
#include "parser_expr_binary.h"

static expr_t *parse_expression(parser_t *p);
static expr_t *parse_assignment(parser_t *p);
static expr_t *parse_conditional(parser_t *p);

typedef enum {
    AOP_NONE,
    AOP_ASSIGN,
    AOP_ADD,
    AOP_SUB,
    AOP_MUL,
    AOP_DIV,
    AOP_MOD,
    AOP_AND,
    AOP_OR,
    AOP_XOR,
    AOP_SHL,
    AOP_SHR
} assign_op_t;

static assign_op_t consume_assign_op(parser_t *p, size_t *line, size_t *column);
static binop_t binop_from_assign(assign_op_t op);
static expr_t *create_assignment_node(expr_t *left, expr_t *right,
                                      size_t line, size_t column);
static expr_t *make_assignment(expr_t *left, expr_t *right,
                               assign_op_t op, size_t line, size_t column);

/* Parse conditional expressions with ?: */
static expr_t *parse_conditional(parser_t *p)
{
    expr_t *cond = parse_logical_or(p);
    if (!cond)
        return NULL;

    if (match(p, TOK_QMARK)) {
        expr_t *then_expr = parse_expression(p);
        if (!then_expr || !match(p, TOK_COLON)) {
            ast_free_expr(cond);
            ast_free_expr(then_expr);
            return NULL;
        }
        expr_t *else_expr = parse_conditional(p);
        if (!else_expr) {
            ast_free_expr(cond);
            ast_free_expr(then_expr);
            return NULL;
        }
        expr_t *res = ast_make_cond(cond, then_expr, else_expr,
                                    cond->line, cond->column);
        return res;
    }
    return cond;
}

static assign_op_t consume_assign_op(parser_t *p, size_t *line, size_t *column)
{
    token_t *tok = peek(p);
    if (!tok)
        return AOP_NONE;

    assign_op_t op;
    switch (tok->type) {
    case TOK_ASSIGN:      op = AOP_ASSIGN; break;
    case TOK_PLUSEQ:      op = AOP_ADD;    break;
    case TOK_MINUSEQ:     op = AOP_SUB;    break;
    case TOK_STAREQ:      op = AOP_MUL;    break;
    case TOK_SLASHEQ:     op = AOP_DIV;    break;
    case TOK_PERCENTEQ:   op = AOP_MOD;    break;
    case TOK_AMPEQ:       op = AOP_AND;    break;
    case TOK_PIPEEQ:      op = AOP_OR;     break;
    case TOK_CARETEQ:     op = AOP_XOR;    break;
    case TOK_SHLEQ:       op = AOP_SHL;    break;
    case TOK_SHREQ:       op = AOP_SHR;    break;
    default:
        return AOP_NONE;
    }

    if (line)
        *line = tok->line;
    if (column)
        *column = tok->column;
    p->pos++;
    return op;
}

static binop_t binop_from_assign(assign_op_t op)
{
    switch (op) {
    case AOP_ADD: return BINOP_ADD;
    case AOP_SUB: return BINOP_SUB;
    case AOP_MUL: return BINOP_MUL;
    case AOP_DIV: return BINOP_DIV;
    case AOP_MOD: return BINOP_MOD;
    case AOP_AND: return BINOP_BITAND;
    case AOP_OR:  return BINOP_BITOR;
    case AOP_XOR: return BINOP_BITXOR;
    case AOP_SHL: return BINOP_SHL;
    case AOP_SHR: return BINOP_SHR;
    default:           return (binop_t)-1;
    }
}

static expr_t *create_assignment_node(expr_t *left, expr_t *right,
                                      size_t line, size_t column)
{
    expr_t *res = NULL;
    if (left->kind == EXPR_IDENT) {
        char *name = left->ident.name;
        free(left);
        res = ast_make_assign(name, right, line, column);
        free(name);
    } else if (left->kind == EXPR_INDEX) {
        expr_t *arr = left->index.array;
        expr_t *idx = left->index.index;
        free(left);
        res = ast_make_assign_index(arr, idx, right, line, column);
    } else {
        expr_t *obj = left->member.object;
        char *mem = left->member.member;
        int via_ptr = left->member.via_ptr;
        free(left);
        res = ast_make_assign_member(obj, mem, right, via_ptr,
                                     line, column);
        free(mem);
    }

    return res;
}

static expr_t *make_assignment(expr_t *left, expr_t *right,
                               assign_op_t op, size_t line, size_t column)
{
    if (left->kind != EXPR_IDENT && left->kind != EXPR_INDEX &&
        left->kind != EXPR_MEMBER) {
        ast_free_expr(left);
        ast_free_expr(right);
        return NULL;
    }

    if (op != AOP_ASSIGN && op != AOP_NONE) {
        binop_t bop = binop_from_assign(op);
        if (bop == (binop_t)-1) {
            ast_free_expr(left);
            ast_free_expr(right);
            return NULL;
        }

        expr_t *lhs_copy = clone_expr(left);
        if (!lhs_copy) {
            ast_free_expr(left);
            ast_free_expr(right);
            return NULL;
        }

        right = ast_make_binary(bop, lhs_copy, right, line, column);
    }

    return create_assignment_node(left, right, line, column);
}

static expr_t *parse_assignment(parser_t *p)
{
    expr_t *left = parse_conditional(p);
    if (!left)
        return NULL;

    size_t line = 0, column = 0;
    assign_op_t op = consume_assign_op(p, &line, &column);
    if (op == AOP_NONE)
        return left;

    expr_t *right = parse_assignment(p);
    if (!right) {
        ast_free_expr(left);
        return NULL;
    }

    return make_assignment(left, right, op, line, column);
}

static expr_t *parse_expression(parser_t *p)
{
    return parse_assignment(p);
}

expr_t *parser_parse_expr(parser_t *p)
{
    return parse_expression(p);
}

