#include <stdlib.h>
#include "parser.h"
#include "parser_types.h"
#include "ast_expr.h"
#include "vector.h"
#include "util.h"

init_entry_t *parser_parse_init_list(parser_t *p, size_t *out_count)
{
    if (!match(p, TOK_LBRACE))
        return NULL;
    vector_t vals_v;
    vector_init(&vals_v, sizeof(init_entry_t));
    if (!match(p, TOK_RBRACE)) {
        do {
            init_entry_t e = { INIT_SIMPLE, NULL, NULL, NULL };
            if (match(p, TOK_DOT)) {
                token_t *id = peek(p);
                if (!id || id->type != TOK_IDENT) {
                    vector_free(&vals_v);
                    return NULL;
                }
                p->pos++;
                e.kind = INIT_FIELD;
                e.field = vc_strdup(id->lexeme);
                if (!e.field) {
                    vector_free(&vals_v);
                    return NULL;
                }
                if (!match(p, TOK_ASSIGN)) {
                    free(e.field);
                    vector_free(&vals_v);
                    return NULL;
                }
                e.value = parser_parse_expr(p);
                if (!e.value) {
                    free(e.field);
                    vector_free(&vals_v);
                    return NULL;
                }
            } else if (match(p, TOK_LBRACKET)) {
                e.kind = INIT_INDEX;
                e.index = parser_parse_expr(p);
                if (!e.index || !match(p, TOK_RBRACKET) || !match(p, TOK_ASSIGN)) {
                    ast_free_expr(e.index);
                    vector_free(&vals_v);
                    return NULL;
                }
                e.value = parser_parse_expr(p);
                if (!e.value) {
                    ast_free_expr(e.index);
                    vector_free(&vals_v);
                    return NULL;
                }
            } else {
                e.value = parser_parse_expr(p);
                if (!e.value) {
                    vector_free(&vals_v);
                    return NULL;
                }
            }

            if (!vector_push(&vals_v, &e)) {
                ast_free_expr(e.index);
                ast_free_expr(e.value);
                free(e.field);
                for (size_t i = 0; i < vals_v.count; i++) {
                    init_entry_t *it = &((init_entry_t *)vals_v.data)[i];
                    ast_free_expr(it->index);
                    ast_free_expr(it->value);
                    free(it->field);
                }
                vector_free(&vals_v);
                return NULL;
            }
        } while (match(p, TOK_COMMA));
        if (!match(p, TOK_RBRACE)) {
            for (size_t i = 0; i < vals_v.count; i++) {
                init_entry_t *it = &((init_entry_t *)vals_v.data)[i];
                ast_free_expr(it->index);
                ast_free_expr(it->value);
                free(it->field);
            }
            vector_free(&vals_v);
            return NULL;
        }
    }
    if (out_count)
        *out_count = vals_v.count;
    return (init_entry_t *)vals_v.data;
}
