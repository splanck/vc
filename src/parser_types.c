/*
 * Helper routines for parsing type specifiers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include "parser_types.h"

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
    } else {
        p->pos = save;
        return 0;
    }
    if (out)
        *out = t;
    return 1;
}

size_t basic_type_size(type_kind_t t)
{
    switch (t) {
    case TYPE_CHAR: case TYPE_UCHAR: case TYPE_BOOL:
        return 1;
    case TYPE_SHORT: case TYPE_USHORT:
        return 2;
    case TYPE_INT: case TYPE_UINT: case TYPE_LONG: case TYPE_ULONG:
        return 4;
    case TYPE_LLONG: case TYPE_ULLONG:
        return 8;
    case TYPE_LDOUBLE:
        return 10;
    default:
        return 4;
    }
}

