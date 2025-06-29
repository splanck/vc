/*
 * Helper routines for parsing type specifiers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include "parser_types.h"
#include "vector.h"
#include <string.h>
#include <stdlib.h>

/* Parse a fundamental type specifier possibly prefixed with 'unsigned'. */
int parse_basic_type(parser_t *p, type_kind_t *out)
{
    size_t save = p->pos;
    int is_unsigned = match(p, TOK_KW_UNSIGNED);
    type_kind_t t;
    if (match(p, TOK_KW_SHORT))
        t = is_unsigned ? TYPE_USHORT : TYPE_SHORT;
    else if (match(p, TOK_KW_LONG)) {
        if (match(p, TOK_KW_DOUBLE))
            t = TYPE_LDOUBLE;
        else if (match(p, TOK_KW_LONG))
            t = is_unsigned ? TYPE_ULLONG : TYPE_LLONG;
        else
            t = is_unsigned ? TYPE_ULONG : TYPE_LONG;
    } else if (match(p, TOK_KW_BOOL)) {
        t = TYPE_BOOL;
    } else if (match(p, TOK_KW_INT)) {
        t = is_unsigned ? TYPE_UINT : TYPE_INT;
    } else if (match(p, TOK_KW_CHAR)) {
        t = TYPE_CHAR;
    } else if (match(p, TOK_KW_FLOAT)) {
        t = TYPE_FLOAT;
    } else if (match(p, TOK_KW_DOUBLE)) {
        t = TYPE_DOUBLE;
    } else if (match(p, TOK_KW_VOID)) {
        t = TYPE_VOID;
    } else if (match(p, TOK_KW_ENUM)) {
        token_t *id = peek(p);
        if (!id || id->type != TOK_IDENT) {
            p->pos = save;
            return 0;
        }
        p->pos++;
        (void)is_unsigned;
        t = TYPE_ENUM;
    } else {
        if (is_unsigned) {
            t = TYPE_UINT;
        } else {
            p->pos = save;
            return 0;
        }
    }
    if (out)
        *out = t;
    return 1;
}

/* Return the size in bytes for the given fundamental type. */
size_t basic_type_size(type_kind_t t)
{
    switch (t) {
    case TYPE_CHAR: case TYPE_UCHAR: case TYPE_BOOL:
        return 1;
    case TYPE_SHORT: case TYPE_USHORT:
        return 2;
    case TYPE_INT: case TYPE_UINT: case TYPE_LONG: case TYPE_ULONG:
        return 4;
    case TYPE_FLOAT:
        return 4;
    case TYPE_DOUBLE:
        return 8;
    case TYPE_LLONG: case TYPE_ULLONG:
        return 8;
    case TYPE_LDOUBLE:
        return 10;
    default:
        return 4;
    }
}

/* Attempt to parse a function pointer suffix like '(*name)(int, float)'. */
int parse_func_ptr_suffix(parser_t *p, char **name,
                          type_kind_t **param_types, size_t *param_count,
                          int *is_variadic)
{
    size_t start = p->pos;
    if (!match(p, TOK_LPAREN))
        return 0;
    if (!match(p, TOK_STAR)) {
        p->pos = start;
        return 0;
    }
    token_t *id = peek(p);
    if (!id || id->type != TOK_IDENT) {
        p->pos = start;
        return 0;
    }
    p->pos++;
    if (!match(p, TOK_RPAREN)) {
        p->pos = start;
        return 0;
    }
    if (!match(p, TOK_LPAREN)) {
        p->pos = start;
        return 0;
    }

    vector_t params_v;
    vector_init(&params_v, sizeof(type_kind_t));
    int variadic = 0;

    if (!match(p, TOK_RPAREN)) {
        do {
            if (match(p, TOK_ELLIPSIS)) {
                variadic = 1;
                break;
            }
            type_kind_t pt;
            if (!parse_basic_type(p, &pt)) {
                vector_free(&params_v);
                p->pos = start;
                return 0;
            }
            if (match(p, TOK_STAR)) {
                pt = TYPE_PTR;
                match(p, TOK_KW_RESTRICT);
            }
            token_t *tmp = peek(p);
            if (tmp && tmp->type == TOK_IDENT)
                p->pos++; /* optional name */
            if (!vector_push(&params_v, &pt)) {
                vector_free(&params_v);
                p->pos = start;
                return 0;
            }
        } while (match(p, TOK_COMMA));
        if (!match(p, TOK_RPAREN)) {
            vector_free(&params_v);
            p->pos = start;
            return 0;
        }
    }

    *name = id->lexeme;
    *param_count = params_v.count;
    if (params_v.count) {
        *param_types = malloc(params_v.count * sizeof(type_kind_t));
        if (!*param_types) {
            vector_free(&params_v);
            *param_types = NULL;
            *param_count = 0;
            p->pos = start;
            return 0;
        }
        memcpy(*param_types, params_v.data,
               params_v.count * sizeof(type_kind_t));
    } else {
        *param_types = NULL;
    }
    *is_variadic = variadic;
    vector_free(&params_v);
    return 1;
}

