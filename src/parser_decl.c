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

/* Variable declaration beginning at the current token. */
stmt_t *parser_parse_var_decl(parser_t *p)
{
    int is_extern = match(p, TOK_KW_EXTERN);
    int is_static = match(p, TOK_KW_STATIC);
    int is_register = match(p, TOK_KW_REGISTER);
    match(p, TOK_KW_INLINE);
    int is_const = match(p, TOK_KW_CONST);
    int is_volatile = match(p, TOK_KW_VOLATILE);
    token_t *kw_tok = peek(p);
    type_kind_t t;
    char *tag_name = NULL;
    size_t elem_size = 0;
    if (match(p, TOK_KW_UNION)) {
        token_t *tag_tok = peek(p);
        if (!tag_tok || tag_tok->type != TOK_IDENT)
            return NULL;
        p->pos++;
        tag_name = vc_strdup(tag_tok->lexeme);
        if (!tag_name)
            return NULL;
        t = TYPE_UNION;
    } else {
        if (!parse_basic_type(p, &t))
            return NULL;
        elem_size = basic_type_size(t);
    }
    int is_restrict = 0;
    if (match(p, TOK_STAR)) {
        t = TYPE_PTR;
        is_restrict = match(p, TOK_KW_RESTRICT);
    }
    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT)
        return NULL;
    p->pos++;
    char *name = tok->lexeme;
    size_t arr_size = 0;
    expr_t *size_expr = NULL;
    if (match(p, TOK_LBRACKET)) {
        size_t save = p->pos;
        if (match(p, TOK_RBRACKET)) {
            t = TYPE_ARRAY;
        } else {
            size_expr = parser_parse_expr(p);
            if (!size_expr || !match(p, TOK_RBRACKET)) {
                ast_free_expr(size_expr);
                p->pos = save;
                return NULL;
            }
            if (size_expr->kind == EXPR_NUMBER)
                arr_size = strtoul(size_expr->number.value, NULL, 10);
            t = TYPE_ARRAY;
        }
    }
    expr_t *init = NULL;
    init_entry_t *init_list = NULL;
    size_t init_count = 0;
    if (match(p, TOK_ASSIGN)) {
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
                return NULL;
            }
        } else {
            init = parser_parse_expr(p);
            if (!init || !match(p, TOK_SEMI)) {
                ast_free_expr(init);
                return NULL;
            }
        }
    } else {
        if (!match(p, TOK_SEMI))
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
    vector_init(&members_v, sizeof(union_member_t));
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
        union_member_t m = { vc_strdup(id->lexeme), mt, mem_sz, 0 };
        if (!vector_push(&members_v, &m)) {
            free(m.name);
            goto fail;
        }
    }

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
    vector_init(&members_v, sizeof(union_member_t));
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
        union_member_t m = { vc_strdup(id->lexeme), mt, mem_sz, 0 };
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

