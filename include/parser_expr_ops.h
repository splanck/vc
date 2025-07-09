#ifndef VC_PARSER_EXPR_OPS_H
#define VC_PARSER_EXPR_OPS_H

#include "parser.h"
#include "parser_types.h"
#include "ast_expr.h"

expr_t *parse_prefix_expr(parser_t *p);
expr_t *parse_cast(parser_t *p);
int parse_type(parser_t *p, type_kind_t *out_type, size_t *out_size,
               size_t *elem_size);
expr_t *parse_postfix_expr(parser_t *p);

#endif /* VC_PARSER_EXPR_OPS_H */
