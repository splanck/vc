/*
 * Enum declaration parsing helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "parser.h"
#include "vector.h"
#include "util.h"
#include "parser_types.h"
#include "ast_stmt.h"
#include "ast_expr.h"
#include "error.h"
#include "parser_decl_enum.h"

/* Parse an enum declaration */
stmt_t *parser_parse_enum_decl(parser_t *p)
{
    token_t *kw = &p->tokens[p->pos - 1];
    token_t *tok = peek(p);
    char *tag = NULL;
    if (tok && tok->type == TOK_IDENT) {
        p->pos++;
        tag = tok->lexeme;
    }
    if (!match(p, TOK_LBRACE))
        return NULL;

    vector_t items_v;
    vector_init(&items_v, sizeof(enumerator_t));
    int ok = 0;
    do {
        tok = peek(p);
        if (!tok || tok->type != TOK_IDENT)
            goto fail;
        p->pos++;
        char *name = vc_strdup(tok->lexeme);
        if (!name)
            goto fail;
        expr_t *val = NULL;
        if (match(p, TOK_ASSIGN)) {
            val = parser_parse_expr(p);
            if (!val) {
                free(name);
                goto fail;
            }
        }
        enumerator_t tmp = { name, val };
        if (!vector_push(&items_v, &tmp)) {
            free(name);
            ast_free_expr(val);
            goto fail;
        }
    } while (match(p, TOK_COMMA));

    if (!match(p, TOK_RBRACE) || !match(p, TOK_SEMI))
        goto fail;

    ok = 1;
fail:
    if (!ok) {
        for (size_t i = 0; i < items_v.count; i++) {
            enumerator_t *it = &((enumerator_t *)items_v.data)[i];
            free(it->name);
            ast_free_expr(it->value);
        }
        free(items_v.data);
        return NULL;
    }
    enumerator_t *items = (enumerator_t *)items_v.data;
    size_t count = items_v.count;
    stmt_t *stmt = ast_make_enum_decl(tag, items, count, kw->line, kw->column);
    if (!stmt) {
        for (size_t i = 0; i < count; i++) {
            free(items[i].name);
            ast_free_expr(items[i].value);
        }
        free(items);
        return NULL;
    }
    return stmt;
}

