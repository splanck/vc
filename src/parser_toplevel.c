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
#include "parser_decl_var.h"
#include "parser_decl_struct.h"
#include "parser_decl_enum.h"
#include "symtable.h"
#include "util.h"
#include "error.h"
#include <string.h>
/* external function parser */
int parse_function_or_var(parser_t *p, symtable_t *funcs,
    int is_extern, int is_static, int is_register,
    int is_const, int is_volatile, int is_inline,
    int is_noreturn, size_t spec_pos, size_t line, size_t column,
    func_t **out_func, stmt_t **out_global);


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
            error_set(&error_ctx, num->line, num->column, NULL, NULL);
            error_print(&error_ctx, "Integer constant out of range");
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
/* Parse the parameter list of a function prototype.  The parser must be
 * positioned just after the opening '('.  Parsed parameter types and sizes are
 * stored in the provided vectors which must be initialized by this function. */

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
    int is_noreturn = match(p, TOK_KW_NORETURN);
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
                                     is_const, is_volatile, is_inline, is_noreturn,
                                     spec_pos,
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

    if (tok->type == TOK_KW_STATIC_ASSERT) {
        p->pos = spec_pos;
        if (out_global)
            *out_global = parser_parse_static_assert(p);
        else
            parser_parse_static_assert(p);
        return 1;
    }

    if (tok->type == TOK_KW_TYPEDEF)
        return parse_typedef_decl(p, start, out_global);

    p->pos = spec_pos;
    return parse_function_or_var(p, funcs, is_extern, is_static, is_register,
                                 is_const, is_volatile, is_inline, is_noreturn,
                                 spec_pos,
                                 tok->line, tok->column,
                                 out_func, out_global);
}

