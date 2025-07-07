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
#include "symtable.h"

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

/* Parse a single statement at the current position and advance past it.
 * Returns the constructed stmt_t or NULL on failure. */
stmt_t *parser_parse_stmt(parser_t *p);

/* Parse a top-level declaration.  Exactly one of out_func or out_global
 * will be set on success.  The return value is non-zero if a valid
 * declaration was consumed. */
int parser_parse_toplevel(parser_t *p, symtable_t *funcs,
                          func_t **out_func, stmt_t **out_global);

/* Entry point for expression parsing. */
expr_t *parser_parse_expr(parser_t *p);

/* Parse an initializer list between '{' and '}'.  The number of parsed
 * expressions is stored in out_count. */
/* Parse an initializer list enclosed in braces and return the entries. */
init_entry_t *parser_parse_init_list(parser_t *p, size_t *out_count);

/* Parse a variable declaration at either global or local scope. */
stmt_t *parser_parse_var_decl(parser_t *p);

/* Parse an enum type definition. */
stmt_t *parser_parse_enum_decl(parser_t *p);

/* Parse a union type definition. */
stmt_t *parser_parse_union_decl(parser_t *p);

/* Parse a union variable declaration with inline members. */
stmt_t *parser_parse_union_var_decl(parser_t *p);

/* Parse a struct type definition. */
stmt_t *parser_parse_struct_decl(parser_t *p);

/* Parse a struct variable declaration with inline members. */
stmt_t *parser_parse_struct_var_decl(parser_t *p);

/* Parse a _Static_assert declaration. */
stmt_t *parser_parse_static_assert(parser_t *p);

#endif /* VC_PARSER_H */
