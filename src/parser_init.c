/*
 * Initializer list parsing helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "parser.h"
#include "parser_types.h"
#include "ast_expr.h"
#include "vector.h"
#include "util.h"

/* Forward declarations */
static int parse_field_entry(parser_t *p, init_entry_t *out);
static int parse_index_entry(parser_t *p, init_entry_t *out);
static int parse_simple_entry(parser_t *p, init_entry_t *out);
static void free_init_vector(vector_t *v);

static int parse_field_entry(parser_t *p, init_entry_t *out)
{
    token_t *id = peek(p);
    if (!id || id->type != TOK_IDENT)
        return 0;
    p->pos++;
    out->kind = INIT_FIELD;
    out->field = vc_strdup(id->lexeme);
    if (!out->field)
        return 0;
    if (!match(p, TOK_ASSIGN)) {
        free(out->field);
        out->field = NULL;
        return 0;
    }
    out->value = parser_parse_expr(p);
    if (!out->value) {
        free(out->field);
        out->field = NULL;
        return 0;
    }
    return 1;
}

static int parse_index_entry(parser_t *p, init_entry_t *out)
{
    out->kind = INIT_INDEX;
    out->index = parser_parse_expr(p);
    if (!out->index || !match(p, TOK_RBRACKET) || !match(p, TOK_ASSIGN)) {
        ast_free_expr(out->index);
        out->index = NULL;
        return 0;
    }
    out->value = parser_parse_expr(p);
    if (!out->value) {
        ast_free_expr(out->index);
        out->index = NULL;
        return 0;
    }
    return 1;
}

static int parse_simple_entry(parser_t *p, init_entry_t *out)
{
    out->kind = INIT_SIMPLE;
    out->value = parser_parse_expr(p);
    if (!out->value)
        return 0;
    return 1;
}

static void free_init_vector(vector_t *v)
{
    init_entry_t *it = (init_entry_t *)v->data;
    for (size_t i = 0; i < v->count; i++) {
        ast_free_expr(it[i].index);
        ast_free_expr(it[i].value);
        free(it[i].field);
    }
    vector_free(v);
}

/* Parse a brace-enclosed initializer list.
 * The returned array is heap allocated and out_count receives its length.
 * On any syntax or allocation failure the function frees all allocated
 * entries and returns NULL.
 */

init_entry_t *parser_parse_init_list(parser_t *p, size_t *out_count)
{
    if (!match(p, TOK_LBRACE))
        return NULL;
    vector_t vals_v;
    vector_init(&vals_v, sizeof(init_entry_t));

    if (!match(p, TOK_RBRACE)) {
        do {
            init_entry_t e = { INIT_SIMPLE, NULL, NULL, NULL };
            int ok = 0;

            if (match(p, TOK_DOT))
                ok = parse_field_entry(p, &e);
            else if (match(p, TOK_LBRACKET))
                ok = parse_index_entry(p, &e);
            else
                ok = parse_simple_entry(p, &e);

            if (!ok) {
                free_init_vector(&vals_v);
                return NULL;
            }

            if (!vector_push(&vals_v, &e)) {
                ast_free_expr(e.index);
                ast_free_expr(e.value);
                free(e.field);
                free_init_vector(&vals_v);
                return NULL;
            }
        } while (match(p, TOK_COMMA));

        if (!match(p, TOK_RBRACE)) {
            free_init_vector(&vals_v);
            return NULL;
        }
    }
    if (out_count)
        *out_count = vals_v.count;
    return (init_entry_t *)vals_v.data;
}
