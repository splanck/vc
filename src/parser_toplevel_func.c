/*
 * Top-level function parsing helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "parser_core.h"
#include "vector.h"
#include "parser_types.h"
#include "ast_stmt.h"
#include "ast_expr.h"
#include "symtable.h"
#include "util.h"
#include "error.h"
#include "parser_decl_var.h"

/* external variable parsing helper */
int parse_global_var_init(parser_t *p, const char *name, type_kind_t type,
                          size_t elem_size, int is_static, int is_register,
                          int is_extern, int is_const, int is_volatile,
                          int is_restrict, const char *tag,
                          size_t line, size_t column,
                          stmt_t **out_global);

/* Parse the sequence '__attribute__((noreturn))' if present */
static int parse_gnu_noreturn(parser_t *p)
{
    size_t save = p->pos;
    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT || strcmp(tok->lexeme, "__attribute__") != 0)
        return 0;
    p->pos++;
    if (!match(p, TOK_LPAREN) || !match(p, TOK_LPAREN)) {
        p->pos = save; return 0;
    }
    tok = peek(p);
    if (!tok || tok->type != TOK_IDENT || strcmp(tok->lexeme, "noreturn") != 0) {
        p->pos = save; return 0;
    }
    p->pos++;
    if (!match(p, TOK_RPAREN) || !match(p, TOK_RPAREN)) {
        p->pos = save; return 0;
    }
    return 1;
}

static size_t lookup_aggr_size(symtable_t *tab, type_kind_t t, const char *tag)
{
    if (!tab || !tag)
        return 0;
    symbol_t *sym = NULL;
    if (t == TYPE_STRUCT)
        sym = symtable_lookup_struct(tab, tag);
    else if (t == TYPE_UNION)
        sym = symtable_lookup_union(tab, tag);
    if (!sym)
        return 0;
    return (t == TYPE_STRUCT) ? sym->struct_total_size : sym->total_size;
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
static int parse_param_list_proto(parser_t *p, symtable_t *funcs,
                                  vector_t *types_v, vector_t *sizes_v,
                                  int *is_variadic)
{
    vector_init(types_v, sizeof(type_kind_t));
    vector_init(sizes_v, sizeof(size_t));
    *is_variadic = 0;

    if (!match(p, TOK_RPAREN)) {
        if (match(p, TOK_KW_VOID)) {
            token_t *next = peek(p);
            if (next && next->type == TOK_RPAREN) {
                p->pos++; /* consume ')' */
                goto done_proto;
            }
            p->pos--; /* treat 'void' as normal type */
        }
        do {
            while (1) {
                if (match(p, TOK_KW_CONST))
                    continue;
                if (match(p, TOK_KW_VOLATILE))
                    continue;
                break;
            }
            if (match(p, TOK_ELLIPSIS)) {
                *is_variadic = 1;
                break;
            }

            type_kind_t pt;
            const char *tag = NULL;
            size_t ps;
            if (match(p, TOK_KW_STRUCT) || match(p, TOK_KW_UNION)) {
                token_type_t kw = p->tokens[p->pos - 1].type;
                token_t *id = peek(p);
                if (!id || id->type != TOK_IDENT)
                    return 0;
                p->pos++;
                tag = id->lexeme;
                pt = (kw == TOK_KW_STRUCT) ? TYPE_STRUCT : TYPE_UNION;
                ps = lookup_aggr_size(funcs, pt, tag);
            } else if (parse_basic_type(p, &pt)) {
                ps = basic_type_size(pt);
            } else {
                token_t *id = peek(p);
                if (id && id->type == TOK_IDENT &&
                    parser_decl_var_lookup_typedef(id->lexeme, &pt, &ps)) {
                    p->pos++;
                } else {
                    return 0;
                }
            }

            if (match(p, TOK_STAR)) {
                pt = TYPE_PTR;
                match(p, TOK_KW_RESTRICT);
            }

            token_t *tmp = peek(p);
            if (tmp && tmp->type == TOK_IDENT)
                p->pos++; /* optional name */

            if (!vector_push(types_v, &pt) || !vector_push(sizes_v, &ps))
                return 0;

        } while (match(p, TOK_COMMA));

        if (!match(p, TOK_RPAREN))
            return 0;
    }

done_proto:
    return 1;
}

/* Handle the function-definition case once the parser has determined that the
 * upcoming tokens represent a definition rather than a prototype. */
static int handle_func_definition(parser_t *p, symtable_t *funcs,
                                  size_t spec_pos, int is_inline,
                                  int is_noreturn, func_t **out_func)
{
    p->pos = spec_pos;
    if (out_func)
        *out_func = parser_parse_func(p, funcs, is_inline, is_noreturn);
    else
        parser_parse_func(p, funcs, is_inline, is_noreturn);

    return out_func ? *out_func != NULL : 0;
}

static int parse_func_prototype(parser_t *p, symtable_t *funcs, const char *name,
                                type_kind_t ret_type, const char *ret_tag,
                                size_t spec_pos,
                                int is_inline, int is_noreturn,
                                func_t **out_func)
{
    size_t start = p->pos; /* at '(' */
    p->pos++; /* consume '(' */

    vector_t param_types_v, param_sizes_v;
    int is_variadic = 0;

    if (!parse_param_list_proto(p, funcs, &param_types_v, &param_sizes_v,
                                &is_variadic)) {
        vector_free(&param_types_v);
        vector_free(&param_sizes_v);
        p->pos = start;
        return 0;
    }

    if (parse_gnu_noreturn(p))
        is_noreturn = 1;

    token_t *after = peek(p);
    if (after && after->type == TOK_SEMI) {
        p->pos++; /* ';' */
        size_t rsz = (ret_type == TYPE_STRUCT || ret_type == TYPE_UNION)
                         ? lookup_aggr_size(funcs, ret_type, ret_tag)
                         : 0;
        symtable_add_func(funcs, name, ret_type, rsz,
                          (size_t *)param_sizes_v.data,
                          (type_kind_t *)param_types_v.data,
                          param_types_v.count, is_variadic, 1,
                          is_inline, is_noreturn);
        vector_free(&param_types_v);
        vector_free(&param_sizes_v);
        return 1;
    }

    if (after && after->type == TOK_LBRACE) {
        vector_free(&param_types_v);
        vector_free(&param_sizes_v);
        return handle_func_definition(p, funcs, spec_pos,
                                      is_inline, is_noreturn, out_func);
    }

    vector_free(&param_types_v);
    vector_free(&param_sizes_v);
    p->pos = start;
    return 0;
}

/* Parse a fundamental or struct/union type specifier optionally followed by
 * a '*' pointer suffix.  spec_pos should mark the start position so the
 * parser can rewind on failure.  On success the parsed type information is
 * returned through the out parameters and the parser is left after the
 * optional pointer token. */
static int parse_type_specifier(parser_t *p, size_t spec_pos, type_kind_t *type,
                                size_t *elem_size, int *is_restrict,
                                char **tag_name)
{
    size_t save = spec_pos;
    char *tag = NULL;
    type_kind_t t;
    size_t esz;

    if (match(p, TOK_KW_STRUCT) || match(p, TOK_KW_UNION)) {
        token_type_t kw = p->tokens[p->pos - 1].type;
        token_t *id = peek(p);
        if (!id || id->type != TOK_IDENT) {
            p->pos = save;
            return 0;
        }
        p->pos++;
        tag = vc_strdup(id->lexeme);
        if (!tag) {
            p->pos = save;
            return 0;
        }
        t = (kw == TOK_KW_STRUCT) ? TYPE_STRUCT : TYPE_UNION;
        esz = 0;
    } else if (parse_basic_type(p, &t)) {
        esz = basic_type_size(t);
    } else {
        token_t *id = peek(p);
        if (id && id->type == TOK_IDENT &&
            parser_decl_var_lookup_typedef(id->lexeme, &t, &esz)) {
            p->pos++;
        } else {
            return 0;
        }
    }
    int restr = 0;
    if (match(p, TOK_STAR)) {
        t = TYPE_PTR;
        restr = match(p, TOK_KW_RESTRICT);
    }

    if (type) *type = t;
    if (elem_size) *elem_size = esz;
    if (is_restrict) *is_restrict = restr;
    if (tag_name)
        *tag_name = tag;
    else
        free(tag);

    return 1;
}

/* Parse the base type, optional pointer qualifier and identifier name for a
 * top-level declaration.  On success all parsed information is returned through
 * the out parameters and the parser is positioned just after the identifier. */
static int parse_decl_type_and_name(parser_t *p, size_t spec_pos,
                                    type_kind_t *type, size_t *elem_size,
                                    int *is_restrict, char **tag_name,
                                    const char **name)
{
    if (!parse_type_specifier(p, spec_pos, type, elem_size,
                              is_restrict, tag_name))
        return 0;

    token_t *id = peek(p);
    if (!id || id->type != TOK_IDENT) {
        free(*tag_name);
        *tag_name = NULL;
        p->pos = spec_pos;
        return 0;
    }

    p->pos++;
    if (name)
        *name = id->lexeme;
    return 1;
}

/* Check for and parse a function prototype or definition following an
 * identifier.  The parser must be positioned just past the identifier.
 * If a '(' token is present parse_func_prototype is invoked and the parser is
 * left after the prototype or definition. */
static int parse_function_prototype(parser_t *p, symtable_t *funcs,
                                    const char *name, type_kind_t ret_type,
                                    const char *ret_tag, size_t spec_pos,
                                    int is_inline, int is_noreturn,
                                    func_t **out_func)
{
    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_LPAREN)
        return 0;

    return parse_func_prototype(p, funcs, name, ret_type, ret_tag,
                                spec_pos, is_inline, is_noreturn,
                                out_func);
}

int parse_function_or_var(parser_t *p, symtable_t *funcs,
                                 int is_extern, int is_static, int is_register,
                                 int is_const, int is_volatile, int is_inline,
                                 int is_noreturn,
                                 size_t spec_pos, size_t line, size_t column,
                                 func_t **out_func, stmt_t **out_global)
{
    type_kind_t t;
    size_t elem_size;
    int is_restrict;
    char *tag_name = NULL;
    const char *name;

    if (!parse_decl_type_and_name(p, spec_pos, &t, &elem_size,
                                  &is_restrict, &tag_name, &name))
        return 0;

    if (parse_function_prototype(p, funcs, name, t, tag_name,
                                 spec_pos, is_inline, is_noreturn, out_func)) {
        free(tag_name);
        return 1;
    }

    int rv = parse_global_var_init(p, name, t, elem_size, is_static,
                                   is_register, is_extern, is_const,
                                   is_volatile, is_restrict, tag_name,
                                   line, column, out_global);
    free(tag_name);
    return rv;
}
