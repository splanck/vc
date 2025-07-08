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
#include <string.h>
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
static expr_t *parse_cast(parser_t *p);
static expr_t *parse_preinc(parser_t *p);
static expr_t *parse_predec(parser_t *p);
static expr_t *parse_deref(parser_t *p);
static expr_t *parse_addr(parser_t *p);
static expr_t *parse_neg(parser_t *p);
static expr_t *parse_not(parser_t *p);
static expr_t *parse_sizeof(parser_t *p);
static expr_t *parse_offsetof(parser_t *p);
static int parse_struct_union_tag(parser_t *p, type_kind_t *out_type,
                                  char **out_tag);

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
static binop_t binop_from_assign(assign_op_t op);
static expr_t *create_assignment_node(expr_t *left, expr_t *right,
                                      size_t line, size_t column);

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
/*
 * Parse a possibly concatenated string literal. Adjacent TOK_STRING or
 * TOK_WIDE_STRING tokens are merged into a single expression node.
 */
static expr_t *parse_string_literal(parser_t *p)
{
    token_t *tok = peek(p);
    if (!tok || (tok->type != TOK_STRING && tok->type != TOK_WIDE_STRING))
        return NULL;

    int is_wide = (tok->type == TOK_WIDE_STRING);
    size_t line = tok->line;
    size_t column = tok->column;

    size_t len = strlen(tok->lexeme);
    char *buf = vc_strdup(tok->lexeme);
    if (!buf)
        return NULL;
    p->pos++;

    while (p->pos < p->count) {
        token_t *next = &p->tokens[p->pos];
        if (next->type != tok->type)
            break;
        size_t nlen = strlen(next->lexeme);
        buf = vc_realloc_or_exit(buf, len + nlen + 1);
        memcpy(buf + len, next->lexeme, nlen + 1);
        len += nlen;
        p->pos++;
    }

    expr_t *res = is_wide ?
        ast_make_wstring(buf, line, column) :
        ast_make_string(buf, line, column);
    free(buf);
    return res;
}

static expr_t *parse_literal(parser_t *p)
{
    token_t *tok = peek(p);
    if (!tok)
        return NULL;
    if (match(p, TOK_NUMBER))
        return ast_make_number(tok->lexeme, tok->line, tok->column);
    expr_t *s = parse_string_literal(p);
    if (s)
        return s;
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
        if (strcmp(tok->lexeme, "offsetof") == 0) {
            p->pos++; /* consume identifier */
            return parse_offsetof(p);
        }
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

/* Parse 'struct tag' or 'union tag' and return the tag name. */
static int parse_struct_union_tag(parser_t *p, type_kind_t *out_type,
                                  char **out_tag)
{
    size_t save = p->pos;
    type_kind_t t;
    if (match(p, TOK_KW_STRUCT))
        t = TYPE_STRUCT;
    else if (match(p, TOK_KW_UNION))
        t = TYPE_UNION;
    else
        return 0;

    token_t *id = peek(p);
    if (!id || id->type != TOK_IDENT) {
        p->pos = save;
        return 0;
    }
    p->pos++;
    if (out_type)
        *out_type = t;
    if (out_tag)
        *out_tag = vc_strdup(id->lexeme);
    return 1;
}

/* Parse an offsetof builtin expression. */
static expr_t *parse_offsetof(parser_t *p)
{
    token_t *kw = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LPAREN))
        return NULL;

    type_kind_t t; char *tag = NULL;
    if (!parse_struct_union_tag(p, &t, &tag))
        return NULL;
    if (!match(p, TOK_COMMA)) {
        free(tag);
        return NULL;
    }

    vector_t names_v; vector_init(&names_v, sizeof(char *));
    token_t *id = peek(p);
    if (!id || id->type != TOK_IDENT) {
        vector_free(&names_v); free(tag); return NULL;
    }
    p->pos++;
    char *dup = vc_strdup(id->lexeme);
    if (!dup || !vector_push(&names_v, &dup)) {
        free(dup); vector_free(&names_v); free(tag); return NULL;
    }
    while (match(p, TOK_DOT)) {
        id = peek(p);
        if (!id || id->type != TOK_IDENT) {
            for (size_t i = 0; i < names_v.count; i++)
                free(((char **)names_v.data)[i]);
            vector_free(&names_v); free(tag); return NULL;
        }
        p->pos++;
        dup = vc_strdup(id->lexeme);
        if (!dup || !vector_push(&names_v, &dup)) {
            free(dup);
            for (size_t i = 0; i < names_v.count; i++)
                free(((char **)names_v.data)[i]);
            vector_free(&names_v); free(tag); return NULL;
        }
    }

    if (!match(p, TOK_RPAREN)) {
        for (size_t i = 0; i < names_v.count; i++)
            free(((char **)names_v.data)[i]);
        vector_free(&names_v); free(tag); return NULL;
    }

    char **arr = (char **)names_v.data;
    size_t count = names_v.count;
    expr_t *res = ast_make_offsetof(t, tag, arr, count, kw->line, kw->column);
    if (!res) {
        for (size_t i = 0; i < count; i++)
            free(arr[i]);
        free(arr); free(tag); return NULL;
    }
    return res;
}

/* Parse a cast expression '(type)expr'. */
static expr_t *parse_cast(parser_t *p)
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
    expr_t *cast = parse_cast(p);
    if (cast)
        return cast;
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


/*
 * Consume and classify an assignment operator.  When the next token
 * represents an assignment, the operator kind is returned and the
 * source location stored in \p line and \p column.  Otherwise
 * ``AOP_NONE`` is returned and the parser position is unchanged.
 */
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

/* Map an assignment token to the corresponding binary operator. */
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

/* Create the appropriate assignment expression node based on \p left. */
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

/* Build the AST node for an assignment using \p left and \p right.  The
 * operator kind is provided via \p op along with the source \p line and
 * \p column.  On error all allocated nodes are freed and NULL is
 * returned. */
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

