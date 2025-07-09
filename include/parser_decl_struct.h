#ifndef VC_PARSER_DECL_STRUCT_H
#define VC_PARSER_DECL_STRUCT_H

#include "parser.h"

stmt_t *parser_parse_union_decl(parser_t *p);
stmt_t *parser_parse_union_var_decl(parser_t *p);
stmt_t *parser_parse_struct_decl(parser_t *p);
stmt_t *parser_parse_struct_var_decl(parser_t *p);

#endif /* VC_PARSER_DECL_STRUCT_H */
