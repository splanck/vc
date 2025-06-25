/*
 * Parser state and helper macros.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PARSER_H
#define VC_PARSER_H

#include <stddef.h>
#include "token.h"
#include "ast.h"

/* Parser state */
typedef struct {
    token_t *tokens;
    size_t count;
    size_t pos;
} parser_t;

static inline token_t *peek(parser_t *p)
{
    if (p->pos >= p->count)
        return NULL;
    return &p->tokens[p->pos];
}

static inline int match(parser_t *p, token_type_t type)
{
    token_t *tok = peek(p);
    if (tok && tok->type == type) {
        p->pos++;
        return 1;
    }
    return 0;
}

/* Initialize the parser with a token array.  Resets the position to 0. */
void parser_init(parser_t *p, token_t *tokens, size_t count);

/* Parse a single statement at the current position and advance past it.
 * Returns the constructed stmt_t or NULL on failure. */
stmt_t *parser_parse_stmt(parser_t *p);

/* Parse an entire function definition beginning with its return type.
 * The returned func_t owns all allocated memory. */
func_t *parser_parse_func(parser_t *p);

/* Parse a top-level declaration.  Exactly one of out_func or out_global
 * will be set on success.  The return value is non-zero if a valid
 * declaration was consumed. */
int parser_parse_toplevel(parser_t *p, func_t **out_func, stmt_t **out_global);

/* Entry point for expression parsing. */
expr_t *parser_parse_expr(parser_t *p);

/* Parse an initializer list between '{' and '}'.  The number of parsed
 * expressions is stored in out_count. */
expr_t **parser_parse_init_list(parser_t *p, size_t *out_count);
stmt_t *parser_parse_enum_decl(parser_t *p);

/* Returns non-zero if the parser has reached EOF */
int parser_is_eof(parser_t *p);

/* Print a parser error message showing the unexpected token and a list of
 * expected tokens. The expected token array may be NULL if there are none. */
void parser_print_error(parser_t *p,
                        const token_type_t *expected,
                        size_t expected_count);

#endif /* VC_PARSER_H */
