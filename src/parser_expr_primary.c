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
#include "parser_expr_literal.h"
#include "parser_expr_ops.h"


/* Forward declaration from parser_expr.c */
expr_t *parser_parse_expr(parser_t *p);

static expr_t *parse_offsetof(parser_t *p);
static int parse_struct_union_tag(parser_t *p, type_kind_t *out_type,
                                  char **out_tag);

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
    if (out_tag) {
        *out_tag = vc_strdup(id->lexeme);
        if (!*out_tag)
            return 0;
    }
    return 1;
}

static int parse_offsetof_members(parser_t *p, vector_t *names_v)
{
    token_t *id = peek(p);
    if (!id || id->type != TOK_IDENT)
        return 0;
    p->pos++;
    char *dup = vc_strdup(id->lexeme);
    if (!dup || !vector_push(names_v, &dup)) {
        free(dup);
        return 0;
    }
    while (match(p, TOK_DOT)) {
        id = peek(p);
        if (!id || id->type != TOK_IDENT)
            return 0;
        p->pos++;
        dup = vc_strdup(id->lexeme);
        if (!dup || !vector_push(names_v, &dup)) {
            free(dup);
            return 0;
        }
    }
    return 1;
}

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
    if (!parse_offsetof_members(p, &names_v) || !match(p, TOK_RPAREN)) {
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
expr_t *parse_base_term(parser_t *p)
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

/* Wrapper to start prefix expression parsing. */
expr_t *parse_primary(parser_t *p)
{
    expr_t *cast = parse_cast(p);
    if (cast)
        return cast;
    return parse_prefix_expr(p);
}

