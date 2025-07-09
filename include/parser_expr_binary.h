#ifndef VC_PARSER_EXPR_BINARY_H
#define VC_PARSER_EXPR_BINARY_H

#include "parser.h"
#include "ast_expr.h"

/* Parse the lowest-level binary expression chain (logical OR). */
expr_t *parse_logical_or(parser_t *p);

#endif /* VC_PARSER_EXPR_BINARY_H */
