/*
 * Core parser API.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PARSER_CORE_H
#define VC_PARSER_CORE_H

#include "parser.h"

/* Initialize parser state */
void parser_init(parser_t *p, token_t *tokens, size_t count);

/* Returns non-zero once the parser has consumed all tokens */
int parser_is_eof(parser_t *p);

/* Emit a parser error message showing the unexpected token */
void parser_print_error(parser_t *p,
                        const token_type_t *expected,
                        size_t expected_count);

/* Parse a full function definition beginning with its return type */
func_t *parser_parse_func(parser_t *p, symtable_t *table,
                          int is_inline, int is_noreturn);

#endif /* VC_PARSER_CORE_H */
