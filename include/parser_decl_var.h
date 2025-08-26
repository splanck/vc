#ifndef VC_PARSER_DECL_VAR_H
#define VC_PARSER_DECL_VAR_H

#include "parser.h"
#include "symtable.h"

stmt_t *parser_parse_var_decl(parser_t *p);
stmt_t *parser_parse_static_assert(parser_t *p);
void parser_decl_var_set_typedef_table(symtable_t *tab);
int parser_decl_var_lookup_typedef(const char *name, type_kind_t *type,
                                   size_t *elem_size);

#endif /* VC_PARSER_DECL_VAR_H */
