/*
 * Variable declaration parsing helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "parser.h"
#include "vector.h"
#include "util.h"
#include "parser_types.h"
#include "ast_stmt.h"
#include "ast_expr.h"
#include "error.h"
#include "parser_decl_var.h"

/* Helper prototypes */
static int parse_decl_specs(parser_t *p, int *is_extern, int *is_static,
                            int *is_register, int *is_const, int *is_volatile,
                            int *is_restrict, type_kind_t *type,
                            type_kind_t *base_type,
                            char **tag_name, size_t *elem_size,
                            expr_t **align_expr, token_t **kw_tok);
static void parse_pointer_suffix(parser_t *p, type_kind_t *type,
                                 int *is_restrict);
static void parse_align_spec(parser_t *p, expr_t **align_expr);
static int parse_array_suffix(parser_t *p, type_kind_t *type, char **name,
                              size_t *arr_size, expr_t **size_expr);
static int parse_braced_initializer(parser_t *p, init_entry_t **init_list,
                                    size_t *init_count);
static int parse_expr_initializer(parser_t *p, expr_t **init);
static int parse_initializer(parser_t *p, type_kind_t type, expr_t **init,
                             init_entry_t **init_list, size_t *init_count);

/* Parse an optional '*' pointer suffix followed by 'restrict'. */
static void parse_pointer_suffix(parser_t *p, type_kind_t *type,
                                 int *is_restrict)
{
    *is_restrict = 0;
    if (match(p, TOK_STAR)) {
        *type = TYPE_PTR;
        *is_restrict = match(p, TOK_KW_RESTRICT);
    }
}

/* Parse an optional _Alignas specification. */
static void parse_align_spec(parser_t *p, expr_t **align_expr)
{
    if (align_expr)
        *align_expr = NULL;
    parse_alignas_spec(p, align_expr);
}

/* Parse declaration specifiers like storage class and base type. */
static int parse_decl_specs(parser_t *p, int *is_extern, int *is_static,
                            int *is_register, int *is_const, int *is_volatile,
                            int *is_restrict, type_kind_t *type,
                            type_kind_t *base_type,
                            char **tag_name, size_t *elem_size,
                            expr_t **align_expr, token_t **kw_tok)
{
    *is_extern = match(p, TOK_KW_EXTERN);
    *is_static = match(p, TOK_KW_STATIC);
    *is_register = match(p, TOK_KW_REGISTER);
    match(p, TOK_KW_INLINE);
    *is_const = match(p, TOK_KW_CONST);
    *is_volatile = match(p, TOK_KW_VOLATILE);

    *kw_tok = peek(p);
    *tag_name = NULL;
    *elem_size = 0;

    if (match(p, TOK_KW_UNION)) {
        token_t *tag = peek(p);
        if (!tag || tag->type != TOK_IDENT)
            return 0;
        p->pos++;
        *tag_name = vc_strdup(tag->lexeme);
        if (!*tag_name)
            return 0;
        *type = TYPE_UNION;
        if (base_type)
            *base_type = *type;
    } else if (match(p, TOK_KW_STRUCT)) {
        token_t *tag = peek(p);
        if (!tag || tag->type != TOK_IDENT)
            return 0;
        p->pos++;
        *tag_name = vc_strdup(tag->lexeme);
        if (!*tag_name)
            return 0;
        *type = TYPE_STRUCT;
        if (base_type)
            *base_type = *type;
    } else {
        if (!parse_basic_type(p, type))
            return 0;
        *elem_size = basic_type_size(*type);
        if (base_type)
            *base_type = *type;
    }

    parse_pointer_suffix(p, type, is_restrict);
    parse_align_spec(p, align_expr);
    return 1;
}

/* Parse the identifier and optional array size suffix. */
static int parse_array_suffix(parser_t *p, type_kind_t *type, char **name,
                              size_t *arr_size, expr_t **size_expr)
{
    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT)
        return 0;
    p->pos++;
    *name = tok->lexeme;
    *arr_size = 0;
    *size_expr = NULL;

    if (match(p, TOK_LBRACKET)) {
        size_t save = p->pos;
        if (match(p, TOK_RBRACKET)) {
            *type = TYPE_ARRAY;
        } else {
            *size_expr = parser_parse_expr(p);
            if (!*size_expr || !match(p, TOK_RBRACKET)) {
                ast_free_expr(*size_expr);
                p->pos = save;
                return 0;
            }
            if ((*size_expr)->kind == EXPR_NUMBER) {
                if (!vc_strtoul_size((*size_expr)->number.value, arr_size)) {
                    error_set((*size_expr)->line, (*size_expr)->column,
                              error_current_file, error_current_function);
                    error_print("Integer constant out of range");
                    ast_free_expr(*size_expr);
                    *size_expr = NULL;
                    p->pos = save;
                    return 0;
                }
                ast_free_expr(*size_expr);
                *size_expr = NULL;
            }
            *type = TYPE_ARRAY;
        }
    }
    return 1;
}

/* Parse an initializer list enclosed in braces followed by a semicolon. */
static int parse_braced_initializer(parser_t *p, init_entry_t **init_list,
                                    size_t *init_count)
{
    *init_list = parser_parse_init_list(p, init_count);
    if (!*init_list || !match(p, TOK_SEMI)) {
        if (*init_list) {
            for (size_t i = 0; i < *init_count; i++) {
                ast_free_expr((*init_list)[i].index);
                ast_free_expr((*init_list)[i].value);
                free((*init_list)[i].field);
            }
            free(*init_list);
        }
        return 0;
    }
    return 1;
}

/* Parse a scalar initializer expression followed by a semicolon. */
static int parse_expr_initializer(parser_t *p, expr_t **init)
{
    *init = parser_parse_expr(p);
    if (!*init || !match(p, TOK_SEMI)) {
        ast_free_expr(*init);
        return 0;
    }
    return 1;
}

/* Parse optional initializer and trailing semicolon. */
static int parse_initializer(parser_t *p, type_kind_t type, expr_t **init,
                             init_entry_t **init_list, size_t *init_count)
{
    *init = NULL;
    *init_list = NULL;
    *init_count = 0;

    if (match(p, TOK_ASSIGN)) {
        if ((type == TYPE_ARRAY || type == TYPE_STRUCT) &&
            peek(p) && peek(p)->type == TOK_LBRACE) {
            if (!parse_braced_initializer(p, init_list, init_count))
                return 0;
        } else {
            if (!parse_expr_initializer(p, init))
                return 0;
        }
    } else {
        if (!match(p, TOK_SEMI))
            return 0;
    }
    return 1;
}

/* Parse a _Static_assert statement */
stmt_t *parser_parse_static_assert(parser_t *p)
{
    if (!match(p, TOK_KW_STATIC_ASSERT))
        return NULL;
    token_t *kw = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LPAREN))
        return NULL;
    expr_t *expr = parser_parse_expr(p);
    if (!expr)
        return NULL;
    if (!match(p, TOK_COMMA)) {
        ast_free_expr(expr);
        return NULL;
    }
    token_t *msg = peek(p);
    if (!msg || msg->type != TOK_STRING) {
        ast_free_expr(expr);
        return NULL;
    }
    p->pos++;
    char *str = msg->lexeme;
    if (!match(p, TOK_RPAREN) || !match(p, TOK_SEMI)) {
        ast_free_expr(expr);
        return NULL;
    }
    return ast_make_static_assert(expr, str, kw->line, kw->column);
}

/* Variable declaration beginning at the current token. */
stmt_t *parser_parse_var_decl(parser_t *p)
{
    int is_extern, is_static, is_register, is_const, is_volatile, is_restrict;
    type_kind_t t;
    char *tag_name = NULL;
    size_t elem_size = 0;
    token_t *kw_tok = NULL;

    type_kind_t base_type;
    expr_t *align_expr = NULL;
    if (!parse_decl_specs(p, &is_extern, &is_static, &is_register,
                          &is_const, &is_volatile, &is_restrict, &t,
                          &base_type, &tag_name, &elem_size,
                          &align_expr, &kw_tok))
        return NULL;

    char *name;
    size_t arr_size;
    expr_t *size_expr;
    type_kind_t *param_types = NULL;
    size_t param_count = 0;
    int variadic = 0;
    type_kind_t func_ret_type = TYPE_UNKNOWN;

    if (t == TYPE_PTR && parse_func_ptr_suffix(p, &name,
                                               &param_types, &param_count,
                                               &variadic)) {
        arr_size = 0;
        size_expr = NULL;
        func_ret_type = base_type;
    } else if (!parse_array_suffix(p, &t, &name, &arr_size, &size_expr)) {
        free(tag_name);
        return NULL;
    }

    expr_t *init;
    init_entry_t *init_list;
    size_t init_count;
    if (!parse_initializer(p, t, &init, &init_list, &init_count)) {
        ast_free_expr(size_expr);
        free(tag_name);
        free(param_types);
        return NULL;
    }

    stmt_t *res = ast_make_var_decl(name, t, arr_size, size_expr, align_expr,
                                    elem_size,
                                    is_static, is_register, is_extern,
                                    is_const, is_volatile, is_restrict,
                                    init, init_list, init_count,
                                    tag_name, NULL, 0,
                                    kw_tok->line, kw_tok->column);
    if (!res) {
        free(tag_name);
        free(param_types);
    } else {
        res->var_decl.func_ret_type = func_ret_type;
        res->var_decl.func_param_types = param_types;
        res->var_decl.func_param_count = param_count;
        res->var_decl.func_variadic = variadic;
    }
    return res;
}

