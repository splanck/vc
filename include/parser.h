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

/* Initialize the parser with a token array */
void parser_init(parser_t *p, token_t *tokens, size_t count);

/* Parse a single statement. Returns NULL on error. */
stmt_t *parser_parse_stmt(parser_t *p);

/* Parse a function definition. Returns NULL on error. */
func_t *parser_parse_func(parser_t *p);

/* Parse a top-level declaration (function or global variable).
 * Returns 1 on success with one of out_func/out_global set. */
int parser_parse_toplevel(parser_t *p, func_t **out_func, stmt_t **out_global);

/* Parse an expression starting at the current token. Returns NULL on error. */
expr_t *parser_parse_expr(parser_t *p);

/* Returns non-zero if the parser has reached EOF */
int parser_is_eof(parser_t *p);

/* Print a parser error message showing the unexpected token and a list of
 * expected tokens. The expected token array may be NULL if there are none. */
void parser_print_error(parser_t *p,
                        const token_type_t *expected,
                        size_t expected_count);

#endif /* VC_PARSER_H */
