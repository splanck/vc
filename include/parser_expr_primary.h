#ifndef VC_PARSER_EXPR_PRIMARY_H
#define VC_PARSER_EXPR_PRIMARY_H

#include "parser.h"
#include "parser_types.h"
#include "ast_expr.h"

/* Parse a primary expression including unary operators */
expr_t *parse_primary(parser_t *p);

#endif /* VC_PARSER_EXPR_PRIMARY_H */
