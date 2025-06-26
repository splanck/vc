/*
 * Top-level parsing helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "parser_core.h"
#include "vector.h"
#include "parser_types.h"
#include "ast_stmt.h"
#include "ast_expr.h"

/* Helper to parse enum declarations at global scope */
static int parse_enum_global(parser_t *p, size_t start_pos, stmt_t **out)
{
    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_KW_ENUM)
        return 0;

    token_t *next = &p->tokens[p->pos + 1];
    if (next && next->type == TOK_LBRACE) {
        p->pos = start_pos;
        match(p, TOK_KW_ENUM);
        if (out)
            *out = parser_parse_enum_decl(p);
        else
            parser_parse_enum_decl(p);
        return 1;
    }
    if (next && next->type == TOK_IDENT &&
        p->pos + 2 < p->count && p->tokens[p->pos + 2].type == TOK_LBRACE) {
        p->pos = start_pos;
        match(p, TOK_KW_ENUM);
        if (out)
            *out = parser_parse_enum_decl(p);
        else
            parser_parse_enum_decl(p);
        return 1;
    }

    p->pos = start_pos;
    return 0;
}

/* Helper to parse struct or union declarations/variables at global scope */
static int parse_struct_or_union_global(parser_t *p, size_t start_pos,
                                        stmt_t **out)
{
    token_t *tok = peek(p);
    if (!tok || (tok->type != TOK_KW_STRUCT && tok->type != TOK_KW_UNION))
        return 0;

    token_type_t kw = tok->type;
    token_t *next = &p->tokens[p->pos + 1];
    if (next && next->type == TOK_IDENT &&
        p->pos + 2 < p->count && p->tokens[p->pos + 2].type == TOK_LBRACE) {
        p->pos = start_pos;
        if (kw == TOK_KW_STRUCT) {
            if (out)
                *out = parser_parse_struct_decl(p);
            else
                parser_parse_struct_decl(p);
        } else {
            if (out)
                *out = parser_parse_union_decl(p);
            else
                parser_parse_union_decl(p);
        }
        return 1;
    }

    p->pos = start_pos;
    if (kw == TOK_KW_STRUCT) {
        if (out)
            *out = parser_parse_struct_var_decl(p);
        else
            parser_parse_struct_var_decl(p);
    } else {
        if (out)
            *out = parser_parse_union_var_decl(p);
        else
            parser_parse_union_var_decl(p);
    }
    return 1;
}

/* Helper to parse typedef declarations */
static int parse_typedef_decl(parser_t *p, size_t start_pos, stmt_t **out)
{
    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_KW_TYPEDEF)
        return 0;

    p->pos++; /* consume 'typedef' */
    type_kind_t tt;
    if (!parse_basic_type(p, &tt)) {
        p->pos = start_pos;
        return 0;
    }
    size_t elem_sz = basic_type_size(tt);
    if (match(p, TOK_STAR))
        tt = TYPE_PTR;

    token_t *name_tok = peek(p);
    if (!name_tok || name_tok->type != TOK_IDENT) {
        p->pos = start_pos;
        return 0;
    }
    p->pos++;
    size_t arr_size = 0;
    if (match(p, TOK_LBRACKET)) {
        token_t *num = peek(p);
        if (!num || num->type != TOK_NUMBER) {
            p->pos = start_pos;
            return 0;
        }
        p->pos++;
        arr_size = strtoul(num->lexeme, NULL, 10);
        if (!match(p, TOK_RBRACKET)) {
            p->pos = start_pos;
            return 0;
        }
        tt = TYPE_ARRAY;
    }
    if (!match(p, TOK_SEMI)) {
        p->pos = start_pos;
        return 0;
    }

    if (out)
        *out = ast_make_typedef(name_tok->lexeme, tt, arr_size, elem_sz,
                                tok->line, tok->column);
    return 1;
}

/* Helper to parse a function definition or variable declaration */
static int parse_function_or_var(parser_t *p, symtable_t *funcs,
                                 int is_extern, int is_static, int is_register,
                                 int is_const, int is_volatile,
                                 size_t spec_pos, size_t line, size_t column,
                                 func_t **out_func, stmt_t **out_global)
{
    size_t save = spec_pos;

    type_kind_t t;
    if (!parse_basic_type(p, &t))
        return 0;
    size_t elem_size = basic_type_size(t);
    int is_restrict = 0;
    if (match(p, TOK_STAR)) {
        t = TYPE_PTR;
        is_restrict = match(p, TOK_KW_RESTRICT);
    }

    token_t *id = peek(p);
    if (!id || id->type != TOK_IDENT) {
        p->pos = save;
        return 0;
    }
    p->pos++;

    size_t arr_size = 0;
    token_t *next_tok = peek(p);
    if (next_tok && next_tok->type == TOK_LPAREN) {
        p->pos++; /* '(' */
        vector_t param_types_v;
        vector_init(&param_types_v, sizeof(type_kind_t));
        if (!match(p, TOK_RPAREN)) {
            do {
                type_kind_t pt;
                if (!parse_basic_type(p, &pt)) {
                    vector_free(&param_types_v);
                    p->pos = save;
                    return 0;
                }
                if (match(p, TOK_STAR)) {
                    pt = TYPE_PTR;
                    match(p, TOK_KW_RESTRICT);
                }
                token_t *tmp = peek(p);
                if (tmp && tmp->type == TOK_IDENT)
                    p->pos++; /* optional name */
                if (!vector_push(&param_types_v, &pt)) {
                    vector_free(&param_types_v);
                    p->pos = save;
                    return 0;
                }
            } while (match(p, TOK_COMMA));
            if (!match(p, TOK_RPAREN)) {
                vector_free(&param_types_v);
                p->pos = save;
                return 0;
            }
        }
        token_t *after = peek(p);
        if (after && after->type == TOK_SEMI) {
            p->pos++; /* ';' */
            symtable_add_func(funcs, id->lexeme, t,
                             (type_kind_t *)param_types_v.data,
                             param_types_v.count, 1);
            vector_free(&param_types_v);
            return 1;
        } else if (after && after->type == TOK_LBRACE) {
            vector_free(&param_types_v);
            p->pos = spec_pos;
            if (out_func)
                *out_func = parser_parse_func(p);
            return out_func ? *out_func != NULL : 0;
        } else {
            vector_free(&param_types_v);
            p->pos = save;
            return 0;
        }
    }

    expr_t *size_expr = NULL;
    if (next_tok && next_tok->type == TOK_LBRACKET) {
        p->pos++; /* '[' */
        if (match(p, TOK_RBRACKET)) {
            t = TYPE_ARRAY;
        } else {
            size_expr = parser_parse_expr(p);
            if (!size_expr || !match(p, TOK_RBRACKET)) {
                ast_free_expr(size_expr);
                p->pos = save;
                return 0;
            }
            if (size_expr->kind == EXPR_NUMBER)
                arr_size = strtoul(size_expr->number.value, NULL, 10);
            t = TYPE_ARRAY;
        }
        next_tok = peek(p);
    }

    if (next_tok && next_tok->type == TOK_SEMI) {
        if (t == TYPE_VOID) {
            p->pos = save;
            return 0;
        }
        p->pos++; /* consume ';' */
        if (out_global)
            *out_global = ast_make_var_decl(id->lexeme, t, arr_size, size_expr,
                                           elem_size, is_static, is_register,
                                           is_extern, is_const, is_volatile,
                                           is_restrict, NULL, NULL, 0,
                                           NULL, NULL, 0,
                                           line, column);
        return 1;
    } else if (next_tok && next_tok->type == TOK_ASSIGN) {
        if (t == TYPE_VOID) {
            p->pos = save;
            return 0;
        }
        p->pos++; /* consume '=' */
        expr_t *init = NULL;
        init_entry_t *init_list = NULL;
        size_t init_count = 0;
        if (t == TYPE_ARRAY && peek(p) && peek(p)->type == TOK_LBRACE) {
            init_list = parser_parse_init_list(p, &init_count);
            if (!init_list || !match(p, TOK_SEMI)) {
                if (init_list) {
                    for (size_t i = 0; i < init_count; i++) {
                        ast_free_expr(init_list[i].index);
                        ast_free_expr(init_list[i].value);
                        free(init_list[i].field);
                    }
                    free(init_list);
                }
                p->pos = save;
                return 0;
            }
        } else {
            init = parser_parse_expr(p);
            if (!init || !match(p, TOK_SEMI)) {
                ast_free_expr(init);
                p->pos = save;
                return 0;
            }
        }
        if (out_global)
            *out_global = ast_make_var_decl(id->lexeme, t, arr_size, size_expr,
                                           elem_size, is_static, is_register,
                                           is_extern, is_const, is_volatile,
                                           is_restrict, init, init_list,
                                           init_count, NULL, NULL, 0,
                                           line, column);
        return 1;
    }

    p->pos = spec_pos;
    if (out_func)
        *out_func = parser_parse_func(p);
    return out_func ? *out_func != NULL : 0;
}

/* Parse either a global variable declaration or a full function definition */
int parser_parse_toplevel(parser_t *p, symtable_t *funcs,
                          func_t **out_func, stmt_t **out_global)
{
    if (out_func) *out_func = NULL;
    if (out_global) *out_global = NULL;

    size_t start = p->pos;
    int is_extern = match(p, TOK_KW_EXTERN);
    int is_static = match(p, TOK_KW_STATIC);
    int is_register = match(p, TOK_KW_REGISTER);
    match(p, TOK_KW_INLINE);
    int is_const = match(p, TOK_KW_CONST);
    int is_volatile = match(p, TOK_KW_VOLATILE);
    size_t spec_pos = p->pos;
    token_t *tok = peek(p);
    if (!tok)
        return 0;

    if (tok->type == TOK_KW_STRUCT || tok->type == TOK_KW_UNION) {
        return parse_struct_or_union_global(p, start, out_global);
    }

    if (tok->type == TOK_KW_ENUM) {
        if (parse_enum_global(p, start, out_global))
            return 1;
        tok = peek(p); /* restored by helper */
    }

    if (tok->type == TOK_KW_TYPEDEF)
        return parse_typedef_decl(p, start, out_global);

    p->pos = spec_pos;
    return parse_function_or_var(p, funcs, is_extern, is_static, is_register,
                                 is_const, is_volatile, spec_pos,
                                 tok->line, tok->column,
                                 out_func, out_global);
}

