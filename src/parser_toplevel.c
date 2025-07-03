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
#include "util.h"
#include "error.h"

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
        if (!vc_strtoul_size(num->lexeme, &arr_size)) {
            error_set(num->line, num->column,
                      error_current_file, error_current_function);
            error_print("Integer constant out of range");
            p->pos = start_pos;
            return 0;
        }
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

/* Parse the parameter list of a function and either record a prototype or
 * parse a full definition.
 *
 * On entry the current token must be the '(' following the function name.
 * spec_pos should reference the start of the declaration so the parser can
 * rewind when invoking parser_parse_func for a definition.
 * The parser position will end up just past the terminating ';' for
 * prototypes or at the end of the function definition.  On failure the
 * parser position is left unchanged. */
static int parse_func_prototype(parser_t *p, symtable_t *funcs, const char *name,
                                type_kind_t ret_type, size_t spec_pos,
                                int is_inline, func_t **out_func)
{
    size_t start = p->pos; /* at '(' */
    p->pos++; /* consume '(' */

    vector_t param_types_v, param_sizes_v;
    vector_init(&param_types_v, sizeof(type_kind_t));
    vector_init(&param_sizes_v, sizeof(size_t));
    int is_variadic = 0;

    if (!match(p, TOK_RPAREN)) {
        do {
            if (match(p, TOK_ELLIPSIS)) {
                is_variadic = 1;
                break;
            }
            type_kind_t pt;
            if (!parse_basic_type(p, &pt)) {
                vector_free(&param_types_v);
                vector_free(&param_sizes_v);
                p->pos = start;
                return 0;
            }
            size_t ps = (pt == TYPE_STRUCT || pt == TYPE_UNION) ? 0 : basic_type_size(pt);
            if (match(p, TOK_STAR)) {
                pt = TYPE_PTR;
                match(p, TOK_KW_RESTRICT);
            }
            token_t *tmp = peek(p);
            if (tmp && tmp->type == TOK_IDENT)
                p->pos++; /* optional name */
            if (!vector_push(&param_types_v, &pt) ||
                !vector_push(&param_sizes_v, &ps)) {
                vector_free(&param_types_v);
                vector_free(&param_sizes_v);
                p->pos = start;
                return 0;
            }
        } while (match(p, TOK_COMMA));
        if (!match(p, TOK_RPAREN)) {
            vector_free(&param_types_v);
            vector_free(&param_sizes_v);
            p->pos = start;
            return 0;
        }
    }

    token_t *after = peek(p);
    if (after && after->type == TOK_SEMI) {
        p->pos++; /* ';' */
        size_t rsz = (ret_type == TYPE_STRUCT || ret_type == TYPE_UNION) ? 4 : 0;
        symtable_add_func(funcs, name, ret_type, rsz,
                         (size_t *)param_sizes_v.data,
                         (type_kind_t *)param_types_v.data,
                         param_types_v.count, is_variadic, 1,
                         is_inline);
        vector_free(&param_types_v);
        vector_free(&param_sizes_v);
        return 1;
    } else if (after && after->type == TOK_LBRACE) {
        vector_free(&param_types_v);
        vector_free(&param_sizes_v);
        p->pos = spec_pos;
        if (out_func)
            *out_func = parser_parse_func(p, is_inline);
        return out_func ? *out_func != NULL : 0;
    }

    vector_free(&param_types_v);
    p->pos = start;
    return 0;
}

/* Parse a global variable after its name handling optional array sizes and
 * initializer expressions.  The parser must start immediately after the
 * identifier.  On success the parser is positioned after the terminating
 * semicolon and the created declaration stored in out_global.  The function
 * returns zero and restores the start position on syntax errors. */
/* Parse an optional array size suffix after a variable name.  The parser
 * position is left after the closing ']' or unchanged if no brackets are
 * present.  On syntax errors the parser position is restored to start and
 * zero is returned. */
static int parse_array_size(parser_t *p, type_kind_t *type, size_t *arr_size,
                            expr_t **size_expr)
{
    size_t start = p->pos;
    *arr_size = 0;
    *size_expr = NULL;

    if (match(p, TOK_LBRACKET)) {
        if (match(p, TOK_RBRACKET)) {
            *type = TYPE_ARRAY;
        } else {
            *size_expr = parser_parse_expr(p);
            if (!*size_expr || !match(p, TOK_RBRACKET)) {
                ast_free_expr(*size_expr);
                p->pos = start;
                return 0;
            }
            if ((*size_expr)->kind == EXPR_NUMBER) {
                if (!vc_strtoul_size((*size_expr)->number.value, arr_size)) {
                    error_set((*size_expr)->line, (*size_expr)->column,
                              error_current_file, error_current_function);
                    error_print("Integer constant out of range");
                    ast_free_expr(*size_expr);
                    *size_expr = NULL;
                    p->pos = start;
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

/* Parse an array initializer list followed by a semicolon. */
static int parse_array_initializer(parser_t *p, init_entry_t **init_list,
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

/* Parse an initializer expression followed by a semicolon. */
static int parse_expr_initializer(parser_t *p, expr_t **init)
{
    *init = parser_parse_expr(p);
    if (!*init || !match(p, TOK_SEMI)) {
        ast_free_expr(*init);
        return 0;
    }
    return 1;
}

/* Parse an initializer expression or initializer list followed by a
 * terminating semicolon.  On failure the parser position is restored to
 * start and any allocated expressions are freed. */
static int parse_initializer(parser_t *p, type_kind_t type, expr_t **init,
                             init_entry_t **init_list, size_t *init_count)
{
    size_t start = p->pos;
    *init = NULL;
    *init_list = NULL;
    *init_count = 0;

    if (match(p, TOK_ASSIGN)) {
        int ok;
        if (type == TYPE_ARRAY && peek(p) && peek(p)->type == TOK_LBRACE)
            ok = parse_array_initializer(p, init_list, init_count);
        else
            ok = parse_expr_initializer(p, init);
        if (!ok) {
            p->pos = start;
            return 0;
        }
    } else {
        if (!match(p, TOK_SEMI)) {
            p->pos = start;
            return 0;
        }
    }
    return 1;
}

/* Parse a global variable after its name.  This routine delegates parsing of
 * the optional array size and initializer to helpers so it mostly manages
 * control flow and error recovery.  The parser must start immediately after
 * the identifier.  On success out_global receives the declaration and the
 * parser is positioned after the terminating semicolon. */
static int parse_global_var_init(parser_t *p, const char *name, type_kind_t type,
                                 size_t elem_size, int is_static, int is_register,
                                 int is_extern, int is_const, int is_volatile,
                                 int is_restrict, const char *tag,
                                 size_t line, size_t column,
                                 stmt_t **out_global)
{
    size_t start = p->pos;
    size_t arr_size;
    expr_t *size_expr;

    if (!parse_array_size(p, &type, &arr_size, &size_expr))
        goto fail;

    if (type == TYPE_VOID) {
        ast_free_expr(size_expr);
        goto fail;
    }

    expr_t *init;
    init_entry_t *init_list;
    size_t init_count;

    if (!parse_initializer(p, type, &init, &init_list, &init_count)) {
        ast_free_expr(size_expr);
        goto fail;
    }

    if (out_global)
        *out_global = ast_make_var_decl(name, type, arr_size, size_expr,
                                        elem_size, is_static, is_register,
                                        is_extern, is_const, is_volatile,
                                        is_restrict, init, init_list,
                                        init_count, tag, NULL, 0,
                                        line, column);
    return 1;

fail:
    p->pos = start;
    return 0;
}

/* Helper to parse a function definition or variable declaration */
static int parse_function_or_var(parser_t *p, symtable_t *funcs,
                                 int is_extern, int is_static, int is_register,
                                 int is_const, int is_volatile, int is_inline,
                                 size_t spec_pos, size_t line, size_t column,
                                 func_t **out_func, stmt_t **out_global)
{
    size_t save = spec_pos;

    type_kind_t t;
    char *tag_name = NULL;
    if (match(p, TOK_KW_STRUCT)) {
        token_t *tag = peek(p);
        if (!tag || tag->type != TOK_IDENT) {
            p->pos = save;
            return 0;
        }
        p->pos++;
        tag_name = vc_strdup(tag->lexeme);
        if (!tag_name) {
            p->pos = save;
            return 0;
        }
        t = TYPE_STRUCT;
    } else if (!parse_basic_type(p, &t)) {
        return 0;
    }
    size_t elem_size = (t == TYPE_STRUCT) ? 0 : basic_type_size(t);
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

    token_t *next_tok = peek(p);
    if (next_tok && next_tok->type == TOK_LPAREN) {
        int res = parse_func_prototype(p, funcs, id->lexeme, t, spec_pos,
                                       is_inline, out_func);
        free(tag_name);
        return res;
    }

    int rv = parse_global_var_init(p, id->lexeme, t, elem_size, is_static,
                                   is_register, is_extern, is_const,
                                   is_volatile, is_restrict, tag_name,
                                   line, column, out_global);
    free(tag_name);
    return rv;
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
    int is_inline = match(p, TOK_KW_INLINE);
    int is_const = match(p, TOK_KW_CONST);
    int is_volatile = match(p, TOK_KW_VOLATILE);
    size_t spec_pos = p->pos;
    token_t *tok = peek(p);
    if (!tok)
        return 0;

    if (tok->type == TOK_KW_STRUCT) {
        token_t *next = &p->tokens[p->pos + 1];
        if ((next && next->type == TOK_IDENT &&
             p->pos + 2 < p->count && p->tokens[p->pos + 2].type == TOK_LBRACE) ||
            (next && next->type == TOK_LBRACE)) {
            return parse_struct_or_union_global(p, start, out_global);
        }
        p->pos = spec_pos;
        return parse_function_or_var(p, funcs, is_extern, is_static, is_register,
                                     is_const, is_volatile, is_inline, spec_pos,
                                     tok->line, tok->column,
                                     out_func, out_global);
    }

    if (tok->type == TOK_KW_UNION) {
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
                                 is_const, is_volatile, is_inline, spec_pos,
                                 tok->line, tok->column,
                                 out_func, out_global);
}

