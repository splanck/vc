/*
 * Primary expression parsing helpers.
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
#include "parser_expr_primary.h"


/* Forward declaration from parser_expr.c */
expr_t *parser_parse_expr(parser_t *p);

static expr_t *parse_prefix_expr(parser_t *p);
static expr_t *parse_cast(parser_t *p);
static expr_t *parse_preinc(parser_t *p);
static expr_t *parse_predec(parser_t *p);
static expr_t *parse_deref(parser_t *p);
static expr_t *parse_addr(parser_t *p);
static expr_t *parse_neg(parser_t *p);
static expr_t *parse_not(parser_t *p);
static expr_t *parse_sizeof(parser_t *p);
static expr_t *parse_alignof(parser_t *p);
static expr_t *parse_offsetof(parser_t *p);
static int parse_struct_union_tag(parser_t *p, type_kind_t *out_type,
                                  char **out_tag);
static int parse_type(parser_t *p, type_kind_t *out_type, size_t *out_size,
                      size_t *elem_size);

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
            expr_t *arg = parser_parse_expr(p);
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
    if (tok->type == TOK_NUMBER) {
        size_t save = p->pos;
        token_t *num_tok = tok;
        p->pos++; /* consume number */
        token_t *op_tok = peek(p);
        if (op_tok && (op_tok->type == TOK_PLUS || op_tok->type == TOK_MINUS)) {
            p->pos++; /* consume +/- */
            token_t *imag_tok = peek(p);
            if (imag_tok && imag_tok->type == TOK_IMAG_NUMBER) {
                p->pos++; /* consume imag */
                double real = strtod(num_tok->lexeme, NULL);
                double imag = strtod(imag_tok->lexeme, NULL);
                if (op_tok->type == TOK_MINUS)
                    imag = -imag;
                return ast_make_complex_literal(real, imag,
                                               num_tok->line, num_tok->column);
            }
        }
        p->pos = save;
    }
    if (match(p, TOK_IMAG_NUMBER)) {
        double imag = strtod(tok->lexeme, NULL);
        return ast_make_complex_literal(0.0, imag, tok->line, tok->column);
    }
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
    expr_t *idx = parser_parse_expr(p);
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
        expr_t *expr = parser_parse_expr(p);
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

    return parse_postfix_expr(p);
}

/* Wrapper to start prefix expression parsing. */
expr_t *parse_primary(parser_t *p)
{
    expr_t *cast = parse_cast(p);
    if (cast)
        return cast;
    return parse_prefix_expr(p);
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

