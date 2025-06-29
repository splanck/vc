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
#include "parser_types.h"
#include "ast_expr.h"
#include "vector.h"
#include "util.h"
#include "ast_clone.h"
#include "error.h"

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
static expr_t *parse_prefix_expr(parser_t *p);
static expr_t *parse_preinc(parser_t *p);
static expr_t *parse_predec(parser_t *p);
static expr_t *parse_deref(parser_t *p);
static expr_t *parse_addr(parser_t *p);
static expr_t *parse_neg(parser_t *p);
static expr_t *parse_not(parser_t *p);
static expr_t *parse_sizeof(parser_t *p);
static expr_t *parse_postfix_expr(parser_t *p);
static expr_t *parse_base_term(parser_t *p);
static expr_t *parse_literal(parser_t *p);
static expr_t *parse_identifier_expr(parser_t *p);
static expr_t *parse_compound_literal(parser_t *p);
static expr_t *parse_index_op(parser_t *p, expr_t *base);
static expr_t *parse_member_op(parser_t *p, expr_t *base);
static expr_t *parse_postincdec(parser_t *p, expr_t *base);
static expr_t *parse_call_or_postfix(parser_t *p, expr_t *base);
static int parse_type(parser_t *p, type_kind_t *out_type, size_t *out_size,
                      size_t *elem_size);
static expr_t *parse_logical_and(parser_t *p);
static expr_t *parse_logical_or(parser_t *p);
static expr_t *parse_conditional(parser_t *p);
static int parse_argument_list(parser_t *p, vector_t *out_args);
static binop_t binop_from_assign(token_type_t type);
static expr_t *build_assign_expr(expr_t *left, expr_t *right,
                                 token_t *op_tok);

/* Function pointer type used by parse_binop_chain */
typedef expr_t *(*parse_fn)(parser_t *);

/*
 * Helper to parse left-associative binary operator chains that share the
 * same precedence. The 'sub' parser handles the next higher precedence
 * expression.  'tok_list' contains the tokens that trigger a binary
 * operation and 'op_list' maps each token to the corresponding binop_t
 * value.  On allocation failure or parse error the helper frees any
 * partially built expressions and returns NULL.
 */
static expr_t *parse_binop_chain(parser_t *p, parse_fn sub,
                                 const token_type_t *tok_list,
                                 const binop_t *op_list, size_t count);

/*
 * Free all expr_t pointers stored in a vector and release the vector
 * memory itself.  This is useful for cleaning up partially parsed
 * argument lists on error paths.
 */
static void free_expr_vector(vector_t *v);
static int append_argument(vector_t *v, expr_t *arg);

static void free_expr_vector(vector_t *v)
{
    if (!v)
        return;
    for (size_t i = 0; i < v->count; i++)
        ast_free_expr(((expr_t **)v->data)[i]);
    vector_free(v);
}

/*
 * Push an argument expression onto the vector. On failure the expression
 * is freed and 0 is returned.
 */
static int append_argument(vector_t *v, expr_t *arg)
{
    if (!vector_push(v, &arg)) {
        ast_free_expr(arg);
        return 0;
    }
    return 1;
}

/* Parse a comma-separated argument list enclosed in parentheses. */
static int parse_argument_list(parser_t *p, vector_t *out_args)
{
    if (!match(p, TOK_LPAREN))
        return 0;

    vector_init(out_args, sizeof(expr_t *));

    if (!match(p, TOK_RPAREN)) {
        do {
            expr_t *arg = parse_expression(p);
            if (!arg || !append_argument(out_args, arg)) {
                free_expr_vector(out_args);
                return 0;
            }
        } while (match(p, TOK_COMMA));

        if (!match(p, TOK_RPAREN)) {
            free_expr_vector(out_args);
            return 0;
        }
    }

    return 1;
}

/* Parse numeric, string and character literals. */
static expr_t *parse_literal(parser_t *p)
{
    token_t *tok = peek(p);
    if (!tok)
        return NULL;
    if (match(p, TOK_NUMBER))
        return ast_make_number(tok->lexeme, tok->line, tok->column);
    if (match(p, TOK_STRING))
        return ast_make_string(tok->lexeme, tok->line, tok->column);
    if (match(p, TOK_WIDE_STRING))
        return ast_make_wstring(tok->lexeme, tok->line, tok->column);
    if (match(p, TOK_CHAR))
        return ast_make_char(tok->lexeme[0], tok->line, tok->column);
    if (match(p, TOK_WIDE_CHAR))
        return ast_make_wchar(tok->lexeme[0], tok->line, tok->column);
    return NULL;
}

/* Parse an identifier or function call expression. */
static expr_t *parse_identifier_expr(parser_t *p)
{
    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT)
        return NULL;
    token_t *next = p->pos + 1 < p->count ? &p->tokens[p->pos + 1] : NULL;
    if (next && next->type == TOK_LPAREN) {
        p->pos++; /* consume identifier */
        char *name = tok->lexeme;
        vector_t args_v;
        if (!parse_argument_list(p, &args_v))
            return NULL;
        expr_t **args = (expr_t **)args_v.data;
        size_t count = args_v.count;
        expr_t *call = ast_make_call(name, args, count,
                                     tok->line, tok->column);
        if (!call) {
            free_expr_vector(&args_v);
            return NULL;
        }
        return call;
    }
    match(p, TOK_IDENT);
    return ast_make_ident(tok->lexeme, tok->line, tok->column);
}

/* Parse a single array indexing operation. */
static expr_t *parse_index_op(parser_t *p, expr_t *base)
{
    if (!match(p, TOK_LBRACKET))
        return base;

    token_t *lb = &p->tokens[p->pos - 1];
    expr_t *idx = parse_expression(p);
    if (!idx || !match(p, TOK_RBRACKET)) {
        ast_free_expr(base);
        ast_free_expr(idx);
        return NULL;
    }

    return ast_make_index(base, idx, lb->line, lb->column);
}

/* Parse a single struct/union member access. */
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

/* Parse a single postfix increment or decrement. */
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

/* Apply postfix operations like indexing and member access. */
static expr_t *parse_call_or_postfix(parser_t *p, expr_t *base)
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

/*
 * Parse a compound literal of the form '(type){...}'.
 * On success the returned expression represents the temporary object.
 */
static expr_t *parse_compound_literal(parser_t *p)
{
    size_t save = p->pos;
    if (!match(p, TOK_LPAREN))
        return NULL;

    token_t *lp = &p->tokens[p->pos - 1];
    type_kind_t t; size_t arr_sz; size_t esz;

    if (!parse_type(p, &t, &arr_sz, &esz) || !match(p, TOK_RPAREN) ||
        !peek(p) || peek(p)->type != TOK_LBRACE) {
        p->pos = save;
        return NULL;
    }

    size_t count = 0;
    init_entry_t *list = parser_parse_init_list(p, &count);
    if (!list) {
        p->pos = save;
        return NULL;
    }

    return ast_make_compound(t, arr_sz, esz, NULL, list, count,
                             lp->line, lp->column);
}

/*
 * Parse the most basic expression forms: literals, identifiers, function
 * calls and array indexing.  Prefix unary operators are also handled
 * here.  The returned expr_t represents the parsed sub-expression.
 */
static expr_t *parse_base_term(parser_t *p)
{
    expr_t *base = parse_literal(p);
    if (!base)
        base = parse_identifier_expr(p);
    if (!base)
        base = parse_compound_literal(p);
    if (!base && match(p, TOK_LPAREN)) {
        expr_t *expr = parse_expression(p);
        if (!expr || !match(p, TOK_RPAREN)) {
            ast_free_expr(expr);
            return NULL;
        }
        base = expr;
    }
    return base;
}

/* Apply any postfix operators to a base term. */
static expr_t *parse_postfix_expr(parser_t *p)
{
    expr_t *base = parse_base_term(p);
    if (!base)
        return NULL;
    return parse_call_or_postfix(p, base);
}

/* Prefix operator helpers */
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
    expr_t *e = parse_expression(p);
    if (!e || !match(p, TOK_RPAREN)) {
        ast_free_expr(e);
        return NULL;
    }
    return ast_make_sizeof_expr(e, kw->line, kw->column);
}

/* Handle prefix unary operators before a primary expression. */
static expr_t *parse_prefix_expr(parser_t *p)
{
    static const struct {
        token_type_t tok;
        expr_t *(*fn)(parser_t *);
    } table[] = {
        { TOK_INC,     parse_preinc },
        { TOK_DEC,     parse_predec },
        { TOK_STAR,    parse_deref },
        { TOK_AMP,     parse_addr },
        { TOK_MINUS,   parse_neg },
        { TOK_NOT,     parse_not },
        { TOK_KW_SIZEOF, parse_sizeof }
    };

    token_t *tok = peek(p);
    if (!tok)
        return NULL;

    for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); i++) {
        if (match(p, table[i].tok))
            return table[i].fn(p);
    }

    return parse_postfix_expr(p);
}

/* Wrapper to start prefix expression parsing. */
static expr_t *parse_primary(parser_t *p)
{
    return parse_prefix_expr(p);
}

static expr_t *parse_binop_chain(parser_t *p, parse_fn sub,
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

/*
 * Handle multiplication and division.  The function expects that any
 * higher precedence unary/primary expression has already been consumed
 * and returns the combined binary expression tree.
 */
static expr_t *parse_term(parser_t *p)
{
    static const token_type_t toks[] = { TOK_STAR, TOK_SLASH, TOK_PERCENT };
    static const binop_t ops[] = { BINOP_MUL, BINOP_DIV, BINOP_MOD };
    return parse_binop_chain(p, parse_primary, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Build addition and subtraction expressions. */
static expr_t *parse_additive(parser_t *p)
{
    static const token_type_t toks[] = { TOK_PLUS, TOK_MINUS };
    static const binop_t ops[] = { BINOP_ADD, BINOP_SUB };
    return parse_binop_chain(p, parse_term, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Parse bitwise shift operations '<<' and '>>'. */
static expr_t *parse_shift(parser_t *p)
{
    static const token_type_t toks[] = { TOK_SHL, TOK_SHR };
    static const binop_t ops[] = { BINOP_SHL, BINOP_SHR };
    return parse_binop_chain(p, parse_additive, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Comparison operators <, >, <= and >=. */
static expr_t *parse_relational(parser_t *p)
{
    static const token_type_t toks[] = { TOK_LT, TOK_GT, TOK_LE, TOK_GE };
    static const binop_t ops[] = { BINOP_LT, BINOP_GT, BINOP_LE, BINOP_GE };
    return parse_binop_chain(p, parse_shift, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Parse == and != comparisons. */
static expr_t *parse_equality(parser_t *p)
{
    static const token_type_t toks[] = { TOK_EQ, TOK_NEQ };
    static const binop_t ops[] = { BINOP_EQ, BINOP_NEQ };
    return parse_binop_chain(p, parse_relational, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Parse bitwise AND expressions. */
static expr_t *parse_bitand(parser_t *p)
{
    static const token_type_t toks[] = { TOK_AMP };
    static const binop_t ops[] = { BINOP_BITAND };
    return parse_binop_chain(p, parse_equality, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Parse bitwise XOR expressions. */
static expr_t *parse_bitxor(parser_t *p)
{
    static const token_type_t toks[] = { TOK_CARET };
    static const binop_t ops[] = { BINOP_BITXOR };
    return parse_binop_chain(p, parse_bitand, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Parse bitwise OR expressions. */
static expr_t *parse_bitor(parser_t *p)
{
    static const token_type_t toks[] = { TOK_PIPE };
    static const binop_t ops[] = { BINOP_BITOR };
    return parse_binop_chain(p, parse_bitxor, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Parse logical AND expressions. */
static expr_t *parse_logical_and(parser_t *p)
{
    static const token_type_t toks[] = { TOK_LOGAND };
    static const binop_t ops[] = { BINOP_LOGAND };
    return parse_binop_chain(p, parse_bitor, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

/* Parse logical OR expressions. */
static expr_t *parse_logical_or(parser_t *p)
{
    static const token_type_t toks[] = { TOK_LOGOR };
    static const binop_t ops[] = { BINOP_LOGOR };
    return parse_binop_chain(p, parse_logical_and, toks, ops,
                             sizeof(toks) / sizeof(toks[0]));
}

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


/* Determine if the next token is an assignment operator.  If so the token
 * is consumed, \p compound is set to 1 for "+=", "-=", etc. and 0 for
 * "=".  The consumed token is returned or NULL otherwise. */
static token_t *consume_assign_op(parser_t *p, int *compound)
{
    token_t *tok = peek(p);
    if (!tok)
        return NULL;

    switch (tok->type) {
    case TOK_ASSIGN:
        if (compound)
            *compound = 0;
        p->pos++;
        return tok;
    case TOK_PLUSEQ: case TOK_MINUSEQ: case TOK_STAREQ: case TOK_SLASHEQ:
    case TOK_PERCENTEQ: case TOK_AMPEQ:  case TOK_PIPEEQ:  case TOK_CARETEQ:
    case TOK_SHLEQ:    case TOK_SHREQ:
        if (compound)
            *compound = 1;
        p->pos++;
        return tok;
    default:
        return NULL;
    }
}

/* Map an assignment token to the corresponding binary operator. */
static binop_t binop_from_assign(token_type_t type)
{
    switch (type) {
    case TOK_PLUSEQ:   return BINOP_ADD;
    case TOK_MINUSEQ:  return BINOP_SUB;
    case TOK_STAREQ:   return BINOP_MUL;
    case TOK_SLASHEQ:  return BINOP_DIV;
    case TOK_PERCENTEQ:return BINOP_MOD;
    case TOK_AMPEQ:    return BINOP_BITAND;
    case TOK_PIPEEQ:   return BINOP_BITOR;
    case TOK_CARETEQ:  return BINOP_BITXOR;
    case TOK_SHLEQ:    return BINOP_SHL;
    case TOK_SHREQ:    return BINOP_SHR;
    default:           return (binop_t)-1;
    }
}

/* Create the appropriate assignment expression node based on \p left. */
static expr_t *build_assign_expr(expr_t *left, expr_t *right,
                                 token_t *op_tok)
{
    expr_t *res = NULL;
    if (left->kind == EXPR_IDENT) {
        char *name = left->ident.name;
        free(left);
        res = ast_make_assign(name, right, op_tok->line, op_tok->column);
        free(name);
    } else if (left->kind == EXPR_INDEX) {
        expr_t *arr = left->index.array;
        expr_t *idx = left->index.index;
        free(left);
        res = ast_make_assign_index(arr, idx, right,
                                    op_tok->line, op_tok->column);
    } else {
        expr_t *obj = left->member.object;
        char *mem = left->member.member;
        int via_ptr = left->member.via_ptr;
        free(left);
        res = ast_make_assign_member(obj, mem, right, via_ptr,
                                     op_tok->line, op_tok->column);
        free(mem);
    }

    return res;
}

/* Build the AST node for an assignment using \p left and \p right. The
 * operator token is provided via \p op_tok and \p compound indicates if it
 * was a compound operator.  On error all allocated nodes are freed and NULL
 * is returned. */
static expr_t *make_assignment(expr_t *left, expr_t *right,
                               token_t *op_tok, int compound)
{
    if (left->kind != EXPR_IDENT && left->kind != EXPR_INDEX &&
        left->kind != EXPR_MEMBER) {
        ast_free_expr(left);
        ast_free_expr(right);
        return NULL;
    }

    if (compound) {
        binop_t bop = binop_from_assign(op_tok->type);
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

        right = ast_make_binary(bop, lhs_copy, right,
                                op_tok->line, op_tok->column);
    }

    return build_assign_expr(left, right, op_tok);
}

/* Assignment has the lowest precedence and recurses to itself for chained
 * assignments. */
static expr_t *parse_assignment(parser_t *p)
{
    /*
     * a = b -> EXPR_ASSIGN (or INDEX/MEMBER variants)
     * recurses for chained assignments
     */
    expr_t *left = parse_conditional(p);
    if (!left)
        return NULL;

    int compound = 0;
    token_t *op_tok = consume_assign_op(p, &compound);
    if (!op_tok)
        return left;

    expr_t *right = parse_assignment(p);
    if (!right) {
        ast_free_expr(left);
        return NULL;
    }

    return make_assignment(left, right, op_tok, compound);
}

/* Entry point that parses the full expression grammar. */
static expr_t *parse_expression(parser_t *p)
{
    return parse_assignment(p);
}


/* Public wrapper for expression parsing used by other modules. */
expr_t *parser_parse_expr(parser_t *p)
{
    return parse_expression(p);
}

/* Parse a basic type specification used by sizeof. */
static int parse_type(parser_t *p, type_kind_t *out_type, size_t *out_size,
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
            error_set(num->line, num->column,
                      error_current_file, error_current_function);
            error_print("Integer constant out of range");
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

