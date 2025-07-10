/*
 * Struct and union declaration parsing helpers.
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
#include "parser_decl_struct.h"

/* Parse optional array declarator for a struct/union member */
static int parse_member_array(parser_t *p, int is_union, type_kind_t *type,
                              size_t *arr_size, int *is_flexible)
{
    *arr_size = 0;
    *is_flexible = 0;
    if (!match(p, TOK_LBRACKET))
        return 1;
    if (match(p, TOK_RBRACKET)) {
        if (is_union)
            return 0;
        *type = TYPE_ARRAY;
        *is_flexible = 1;
        return 1;
    }
    token_t *num = peek(p);
    if (!num || num->type != TOK_NUMBER)
        return 0;
    p->pos++;
    if (!vc_strtoul_size(num->lexeme, arr_size)) {
        error_set(num->line, num->column,
                  error_current_file, error_current_function);
        error_print("Integer constant out of range");
        return 0;
    }
    if (!match(p, TOK_RBRACKET))
        return 0;
    *type = TYPE_ARRAY;
    return 1;
}

/* Parse optional bit-field width for a struct/union member */
static int parse_member_bitfield(parser_t *p, type_kind_t type,
                                 unsigned *bit_width)
{
    *bit_width = 0;
    if (!match(p, TOK_COLON))
        return 1;
    if (type == TYPE_ARRAY)
        return 0;
    token_t *num = peek(p);
    if (!num || num->type != TOK_NUMBER)
        return 0;
    p->pos++;
    if (!vc_strtoul_unsigned(num->lexeme, bit_width)) {
        error_set(num->line, num->column,
                  error_current_file, error_current_function);
        error_print("Integer constant out of range");
        return 0;
    }
    return 1;
}

/* Assign parsed member information to the correct output struct */
static void assign_member(int is_union, union_member_t *um,
                          struct_member_t *sm, const char *name,
                          type_kind_t type, size_t elem_size,
                          size_t arr_size, int is_flexible,
                          unsigned bit_width)
{
    size_t mem_sz = elem_size;
    if (type == TYPE_ARRAY)
        mem_sz *= arr_size;

    if (is_union) {
        um->name = (char *)name;
        um->type = type;
        um->elem_size = mem_sz;
        um->offset = 0;
        um->bit_width = bit_width;
        um->bit_offset = 0;
        um->is_flexible = 0;
    } else {
        sm->name = (char *)name;
        sm->type = type;
        sm->elem_size = mem_sz;
        sm->offset = 0;
        sm->bit_width = bit_width;
        sm->bit_offset = 0;
        sm->is_flexible = is_flexible;
    }
}

/* Parse a single struct or union member. */
static int parse_member(parser_t *p, int is_union,
                        union_member_t *um, struct_member_t *sm)
{
    type_kind_t mt;
    if (!parse_basic_type(p, &mt))
        return 0;
    size_t elem_size = basic_type_size(mt);
    if (match(p, TOK_STAR))
        mt = TYPE_PTR;
    token_t *id = peek(p);
    if (!id || id->type != TOK_IDENT)
        return 0;
    p->pos++;

    size_t arr_size = 0;
    int is_flexible = 0;
    if (!parse_member_array(p, is_union, &mt, &arr_size, &is_flexible))
        return 0;

    unsigned bit_width = 0;
    if (!parse_member_bitfield(p, mt, &bit_width))
        return 0;

    if (!match(p, TOK_SEMI))
        return 0;

    char *name = vc_strdup(id->lexeme);
    if (!name)
        return 0;

    assign_member(is_union, um, sm, name, mt, elem_size,
                  arr_size, is_flexible, bit_width);

    return 1;
}

static void free_parsed_members(vector_t *v, int is_union)
{
    if (is_union) {
        union_member_t *m = (union_member_t *)v->data;
        for (size_t i = 0; i < v->count; i++)
            free(m[i].name);
    } else {
        struct_member_t *m = (struct_member_t *)v->data;
        for (size_t i = 0; i < v->count; i++)
            free(m[i].name);
    }
    vector_free(v);
}

/* Parse a sequence of union or struct members enclosed in braces. */
static int parse_member_list(parser_t *p, int is_union, vector_t *members_v)
{
    vector_init(members_v, is_union ? sizeof(union_member_t)
                                    : sizeof(struct_member_t));
    int ok = 0;
    while (!match(p, TOK_RBRACE)) {
        union_member_t um;
        struct_member_t sm;
        if (!parse_member(p, is_union, &um, &sm))
            goto fail;
        if (is_union) {
            if (!vector_push(members_v, &um)) {
                free(um.name);
                goto fail;
            }
        } else {
            if (!vector_push(members_v, &sm)) {
                free(sm.name);
                goto fail;
            }
            if (sm.type == TYPE_ARRAY && sm.elem_size == 0) {
                token_t *next = peek(p);
                if (next && next->type != TOK_RBRACE)
                    goto fail;
            }
        }
    }
    ok = 1;
fail:
    if (!ok)
        free_parsed_members(members_v, is_union);
    return ok;
}

/* Parse a sequence of union members enclosed in braces. */
static int parse_union_members(parser_t *p, vector_t *members_v)
{
    return parse_member_list(p, 1, members_v);
}

/* Common parser for struct/union variable declarations with inline members */
static stmt_t *parse_aggr_var_decl(parser_t *p, int is_union)
{
    int is_extern = match(p, TOK_KW_EXTERN);
    int is_static = match(p, TOK_KW_STATIC);
    int is_register = match(p, TOK_KW_REGISTER);
    int is_const = match(p, TOK_KW_CONST);
    int is_volatile = match(p, TOK_KW_VOLATILE);

    if (!match(p, is_union ? TOK_KW_UNION : TOK_KW_STRUCT))
        return NULL;
    token_t *kw = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LBRACE))
        return NULL;

    vector_t members_v;
    if (!parse_member_list(p, is_union, &members_v))
        return NULL;
    int ok = 0;

    token_t *name_tok = peek(p);
    if (!name_tok || name_tok->type != TOK_IDENT)
        goto fail;
    p->pos++;
    char *name = name_tok->lexeme;
    if (!match(p, TOK_SEMI))
        goto fail;

    ok = 1;
fail:
    if (!ok) {
        free_parsed_members(&members_v, is_union);
        return NULL;
    }
    union_member_t *members = (union_member_t *)members_v.data;
    size_t count = members_v.count;
    stmt_t *res = ast_make_var_decl(name,
                                    is_union ? TYPE_UNION : TYPE_STRUCT,
                                    0, NULL, NULL, 0, is_static,
                                    is_register, is_extern,
                                    is_const, is_volatile, 0,
                                    NULL, NULL, 0,
                                    NULL, members, count,
                                    kw->line, kw->column);
    if (!res) {
        for (size_t i = 0; i < count; i++)
            free(members[i].name);
        free(members);
    }
    return res;
}

/* Parse a union variable with inline member specification */
stmt_t *parser_parse_union_var_decl(parser_t *p)
{
    return parse_aggr_var_decl(p, 1);
}

/* Parse a named union type declaration */
stmt_t *parser_parse_union_decl(parser_t *p)
{
    if (!match(p, TOK_KW_UNION))
        return NULL;
    token_t *kw = &p->tokens[p->pos - 1];
    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT)
        return NULL;
    p->pos++;
    char *tag = tok->lexeme;
    if (!match(p, TOK_LBRACE))
        return NULL;

    vector_t members_v;
    if (!parse_union_members(p, &members_v))
        return NULL;
    int ok = 0;

    if (!match(p, TOK_SEMI))
        goto fail;

    ok = 1;
fail:
    if (!ok) {
        free_parsed_members(&members_v, 1);
        return NULL;
    }
    union_member_t *members = (union_member_t *)members_v.data;
    size_t count = members_v.count;
    return ast_make_union_decl(tag, members, count, kw->line, kw->column);
}

/* Parse a struct variable with inline member specification */
stmt_t *parser_parse_struct_var_decl(parser_t *p)
{
    return parse_aggr_var_decl(p, 0);
}

/* Parse a named struct type declaration */
stmt_t *parser_parse_struct_decl(parser_t *p)
{
    if (!match(p, TOK_KW_STRUCT))
        return NULL;
    token_t *kw = &p->tokens[p->pos - 1];
    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT)
        return NULL;
    p->pos++;
    char *tag = tok->lexeme;
    if (!match(p, TOK_LBRACE))
        return NULL;

    vector_t members_v;
    vector_init(&members_v, sizeof(struct_member_t));
    int ok = 0;
    while (!match(p, TOK_RBRACE)) {
        union_member_t dum;
        struct_member_t m;
        if (!parse_member(p, 0, &dum, &m))
            goto fail;
        if (!vector_push(&members_v, &m)) {
            free(m.name);
            goto fail;
        }
        if (m.type == TYPE_ARRAY && m.elem_size == 0) {
            token_t *next = peek(p);
            if (next && next->type != TOK_RBRACE)
                goto fail;
        }
    }

    if (!match(p, TOK_SEMI))
        goto fail;

    ok = 1;
fail:
    if (!ok) {
        for (size_t i = 0; i < members_v.count; i++)
            free(((struct_member_t *)members_v.data)[i].name);
        vector_free(&members_v);
        return NULL;
    }
    struct_member_t *members = (struct_member_t *)members_v.data;
    size_t count = members_v.count;
    return ast_make_struct_decl(tag, members, count, kw->line, kw->column);
}

