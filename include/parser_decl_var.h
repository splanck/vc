#ifndef VC_PARSER_DECL_VAR_H
#define VC_PARSER_DECL_VAR_H

#include "parser.h"

stmt_t *parser_parse_var_decl(parser_t *p);
stmt_t *parser_parse_static_assert(parser_t *p);

#endif /* VC_PARSER_DECL_VAR_H */
