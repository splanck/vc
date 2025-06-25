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

#endif /* VC_PARSER_TYPES_H */

