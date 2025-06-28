/*
 * Shared parser helpers for type specifiers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PARSER_TYPES_H
#define VC_PARSER_TYPES_H

#include "parser.h"

/* Parse a fundamental type specifier possibly prefixed with 'unsigned'. */
int parse_basic_type(parser_t *p, type_kind_t *out);

/* Return the size in bytes of a basic type. */
size_t basic_type_size(type_kind_t t);

/* Try to parse a function pointer declaration suffix. */
int parse_func_ptr_suffix(parser_t *p, char **name,
                          type_kind_t **param_types, size_t *param_count,
                          int *is_variadic);

#endif /* VC_PARSER_TYPES_H */

