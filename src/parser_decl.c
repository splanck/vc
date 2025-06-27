/*
 * Declaration parsing helpers.
 *
 * Handles variable, struct, union and enum declarations used both at the
 * statement and global scope.
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

/* Helper prototypes */
static int parse_decl_specs(parser_t *p, int *is_extern, int *is_static,
                            int *is_register, int *is_const, int *is_volatile,
                            int *is_restrict, type_kind_t *type,
                            char **tag_name, size_t *elem_size,
                            token_t **kw_tok);
static int parse_array_suffix(parser_t *p, type_kind_t *type, char **name,
                              size_t *arr_size, expr_t **size_expr);
static int parse_initializer(parser_t *p, type_kind_t type, expr_t **init,
                             init_entry_t **init_list, size_t *init_count);

/* Parse a sequence of union or struct members enclosed in braces. */
static int parse_member_list(parser_t *p, int is_union, vector_t *members_v);
/* Parse a sequence of union members enclosed in braces. */
static int parse_union_members(parser_t *p, vector_t *members_v);

/* Parse declaration specifiers like storage class and base type. */
static int parse_decl_specs(parser_t *p, int *is_extern, int *is_static,
                            int *is_register, int *is_const, int *is_volatile,
                            int *is_restrict, type_kind_t *type,
                            char **tag_name, size_t *elem_size,
                            token_t **kw_tok)
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
    } else {
        if (!parse_basic_type(p, type))
            return 0;
        *elem_size = basic_type_size(*type);
    }

    *is_restrict = 0;
    if (match(p, TOK_STAR)) {
        *type = TYPE_PTR;
        *is_restrict = match(p, TOK_KW_RESTRICT);
    }
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
            if ((*size_expr)->kind == EXPR_NUMBER)
                *arr_size = strtoul((*size_expr)->number.value, NULL, 10);
            *type = TYPE_ARRAY;
        }
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
        if (type == TYPE_ARRAY && peek(p) && peek(p)->type == TOK_LBRACE) {
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
        } else {
            *init = parser_parse_expr(p);
            if (!*init || !match(p, TOK_SEMI)) {
                ast_free_expr(*init);
                return 0;
            }
        }
    } else {
        if (!match(p, TOK_SEMI))
            return 0;
    }
    return 1;
}

/* Parse a sequence of union or struct members enclosed in braces. */
static int parse_member_list(parser_t *p, int is_union, vector_t *members_v)
{
    vector_init(members_v, is_union ? sizeof(union_member_t)
                                    : sizeof(struct_member_t));
    int ok = 0;
    while (!match(p, TOK_RBRACE)) {
        type_kind_t mt;
        if (!parse_basic_type(p, &mt))
            goto fail;
        size_t elem_size = basic_type_size(mt);
        if (match(p, TOK_STAR))
            mt = TYPE_PTR;
        token_t *id = peek(p);
        if (!id || id->type != TOK_IDENT)
            goto fail;
        p->pos++;
        size_t arr_size = 0;
        if (match(p, TOK_LBRACKET)) {
            token_t *num = peek(p);
            if (!num || num->type != TOK_NUMBER)
                goto fail;
            p->pos++;
            arr_size = strtoul(num->lexeme, NULL, 10);
            if (!match(p, TOK_RBRACKET))
                goto fail;
            mt = TYPE_ARRAY;
        }
        if (!match(p, TOK_SEMI))
            goto fail;
        size_t mem_sz = elem_size;
        if (mt == TYPE_ARRAY)
            mem_sz *= arr_size;
        if (is_union) {
            union_member_t m = { vc_strdup(id->lexeme), mt, mem_sz, 0 };
            if (!vector_push(members_v, &m)) {
                free(m.name);
                goto fail;
            }
        } else {
            struct_member_t m = { vc_strdup(id->lexeme), mt, mem_sz, 0 };
            if (!vector_push(members_v, &m)) {
                free(m.name);
                goto fail;
            }
        }
    }
    ok = 1;
fail:
    if (!ok) {
        if (is_union) {
            for (size_t i = 0; i < members_v->count; i++)
                free(((union_member_t *)members_v->data)[i].name);
        } else {
            for (size_t i = 0; i < members_v->count; i++)
                free(((struct_member_t *)members_v->data)[i].name);
        }
        vector_free(members_v);
    }
    return ok;
}

/* Variable declaration beginning at the current token. */
stmt_t *parser_parse_var_decl(parser_t *p)
{
    int is_extern, is_static, is_register, is_const, is_volatile, is_restrict;
    type_kind_t t;
    char *tag_name = NULL;
    size_t elem_size = 0;
    token_t *kw_tok = NULL;

    if (!parse_decl_specs(p, &is_extern, &is_static, &is_register,
                          &is_const, &is_volatile, &is_restrict, &t,
                          &tag_name, &elem_size, &kw_tok))
        return NULL;

    char *name;
    size_t arr_size;
    expr_t *size_expr;
    if (!parse_array_suffix(p, &t, &name, &arr_size, &size_expr)) {
        free(tag_name);
        return NULL;
    }

    expr_t *init;
    init_entry_t *init_list;
    size_t init_count;
    if (!parse_initializer(p, t, &init, &init_list, &init_count)) {
        ast_free_expr(size_expr);
        free(tag_name);
        return NULL;
    }

    stmt_t *res = ast_make_var_decl(name, t, arr_size, size_expr, elem_size,
                                    is_static, is_register, is_extern,
                                    is_const, is_volatile, is_restrict,
                                    init, init_list, init_count,
                                    tag_name, NULL, 0,
                                    kw_tok->line, kw_tok->column);
    if (!res)
        free(tag_name);
    return res;
}

/* Parse an enum declaration */
stmt_t *parser_parse_enum_decl(parser_t *p)
{
    token_t *kw = &p->tokens[p->pos - 1];
    token_t *tok = peek(p);
    char *tag = NULL;
    if (tok && tok->type == TOK_IDENT) {
        p->pos++;
        tag = tok->lexeme;
    }
    if (!match(p, TOK_LBRACE))
        return NULL;

    vector_t items_v;
    vector_init(&items_v, sizeof(enumerator_t));
    int ok = 0;
    do {
        tok = peek(p);
        if (!tok || tok->type != TOK_IDENT)
            goto fail;
        p->pos++;
        char *name = vc_strdup(tok->lexeme);
        if (!name)
            goto fail;
        expr_t *val = NULL;
        if (match(p, TOK_ASSIGN)) {
            val = parser_parse_expr(p);
            if (!val) {
                free(name);
                goto fail;
            }
        }
        enumerator_t tmp = { name, val };
        if (!vector_push(&items_v, &tmp)) {
            free(name);
            ast_free_expr(val);
            goto fail;
        }
    } while (match(p, TOK_COMMA));

    if (!match(p, TOK_RBRACE) || !match(p, TOK_SEMI))
        goto fail;

    ok = 1;
fail:
    if (!ok) {
        for (size_t i = 0; i < items_v.count; i++) {
            enumerator_t *it = &((enumerator_t *)items_v.data)[i];
            free(it->name);
            ast_free_expr(it->value);
        }
        free(items_v.data);
        return NULL;
    }
    enumerator_t *items = (enumerator_t *)items_v.data;
    size_t count = items_v.count;
    return ast_make_enum_decl(tag, items, count, kw->line, kw->column);
}

/* Parse a sequence of union members enclosed in braces. */
static int parse_union_members(parser_t *p, vector_t *members_v)
{
    vector_init(members_v, sizeof(union_member_t));
    while (!match(p, TOK_RBRACE)) {
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
        if (match(p, TOK_LBRACKET)) {
            token_t *num = peek(p);
            if (!num || num->type != TOK_NUMBER)
                return 0;
            p->pos++;
            arr_size = strtoul(num->lexeme, NULL, 10);
            if (!match(p, TOK_RBRACKET))
                return 0;
            mt = TYPE_ARRAY;
        }
        if (!match(p, TOK_SEMI))
            return 0;
        size_t mem_sz = elem_size;
        if (mt == TYPE_ARRAY)
            mem_sz *= arr_size;
        union_member_t m = { vc_strdup(id->lexeme), mt, mem_sz, 0 };
        if (!vector_push(members_v, &m)) {
            free(m.name);
            return 0;
        }
    }
    return 1;
}

/* Parse a union variable with inline member specification */
stmt_t *parser_parse_union_var_decl(parser_t *p)
{
    int is_extern = match(p, TOK_KW_EXTERN);
    int is_static = match(p, TOK_KW_STATIC);
    int is_register = match(p, TOK_KW_REGISTER);
    int is_const = match(p, TOK_KW_CONST);
    int is_volatile = match(p, TOK_KW_VOLATILE);
    if (!match(p, TOK_KW_UNION))
        return NULL;
    token_t *kw = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LBRACE))
        return NULL;

    vector_t members_v;
    if (!parse_union_members(p, &members_v)) {
        for (size_t i = 0; i < members_v.count; i++)
            free(((union_member_t *)members_v.data)[i].name);
        vector_free(&members_v);
        return NULL;
    }
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
        for (size_t i = 0; i < members_v.count; i++)
            free(((union_member_t *)members_v.data)[i].name);
        vector_free(&members_v);
        return NULL;
    }
    union_member_t *members = (union_member_t *)members_v.data;
    size_t count = members_v.count;
    stmt_t *res = ast_make_var_decl(name, TYPE_UNION, 0, NULL, 0, is_static,
                                    is_register, is_extern,
                                    is_const, is_volatile, 0, NULL, NULL, 0,
                                    NULL, members, count,
                                    kw->line, kw->column);
    if (!res) {
        for (size_t i = 0; i < count; i++)
            free(members[i].name);
        free(members);
    }
    return res;
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
    if (!parse_union_members(p, &members_v)) {
        for (size_t i = 0; i < members_v.count; i++)
            free(((union_member_t *)members_v.data)[i].name);
        vector_free(&members_v);
        return NULL;
    }
    int ok = 0;

    if (!match(p, TOK_SEMI))
        goto fail;

    ok = 1;
fail:
    if (!ok) {
        for (size_t i = 0; i < members_v.count; i++)
            free(((union_member_t *)members_v.data)[i].name);
        vector_free(&members_v);
        return NULL;
    }
    union_member_t *members = (union_member_t *)members_v.data;
    size_t count = members_v.count;
    return ast_make_union_decl(tag, members, count, kw->line, kw->column);
}

/* Parse a struct variable with inline member specification */
stmt_t *parser_parse_struct_var_decl(parser_t *p)
{
    int is_extern = match(p, TOK_KW_EXTERN);
    int is_static = match(p, TOK_KW_STATIC);
    int is_register = match(p, TOK_KW_REGISTER);
    int is_const = match(p, TOK_KW_CONST);
    int is_volatile = match(p, TOK_KW_VOLATILE);
    if (!match(p, TOK_KW_STRUCT))
        return NULL;
    token_t *kw = &p->tokens[p->pos - 1];
    if (!match(p, TOK_LBRACE))
        return NULL;

    vector_t members_v;
    int ok = 0;
    if (!parse_member_list(p, 0, &members_v))
        return NULL;

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
        for (size_t i = 0; i < members_v.count; i++)
            free(((struct_member_t *)members_v.data)[i].name);
        vector_free(&members_v);
        return NULL;
    }
    struct_member_t *members = (struct_member_t *)members_v.data;
    size_t count = members_v.count;
    stmt_t *res = ast_make_var_decl(name, TYPE_STRUCT, 0, NULL, 0, is_static,
                                    is_register, is_extern,
                                    is_const, is_volatile, 0, NULL, NULL, 0,
                                    NULL, (union_member_t *)members, count,
                                    kw->line, kw->column);
    if (!res) {
        for (size_t i = 0; i < count; i++)
            free(members[i].name);
        free(members);
    }
    return res;
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
        type_kind_t mt;
        if (!parse_basic_type(p, &mt))
            goto fail;
        size_t elem_size = basic_type_size(mt);
        if (match(p, TOK_STAR))
            mt = TYPE_PTR;
        token_t *id = peek(p);
        if (!id || id->type != TOK_IDENT)
            goto fail;
        p->pos++;
        size_t arr_size = 0;
        if (match(p, TOK_LBRACKET)) {
            token_t *num = peek(p);
            if (!num || num->type != TOK_NUMBER)
                goto fail;
            p->pos++;
            arr_size = strtoul(num->lexeme, NULL, 10);
            if (!match(p, TOK_RBRACKET))
                goto fail;
            mt = TYPE_ARRAY;
        }
        if (!match(p, TOK_SEMI))
            goto fail;
        size_t mem_sz = elem_size;
        if (mt == TYPE_ARRAY)
            mem_sz *= arr_size;
        struct_member_t m = { vc_strdup(id->lexeme), mt, mem_sz, 0 };
        if (!vector_push(&members_v, &m)) {
            free(m.name);
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

