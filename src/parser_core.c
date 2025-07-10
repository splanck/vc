/*
 * Core parser utilities and function definition parsing.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include "parser_core.h"
#include "vector.h"
#include "error.h"
#include "parser_types.h"
#include "ast_stmt.h"
#include "ast_expr.h"
#include "util.h"
#include "symtable.h"
#include "token_names.h"
#include <string.h>


/* Helper prototypes */
static void cleanup_param_vectors(vector_t *names_v, vector_t *types_v,
                                  vector_t *sizes_v, vector_t *restrict_v,
                                  vector_t *tags_v);
static int parse_param_decl(parser_t *p, symtable_t *symtab,
                            vector_t *names_v, vector_t *types_v,
                            vector_t *sizes_v, vector_t *tags_v,
                            vector_t *restrict_v);
static int parse_param_list(parser_t *p, symtable_t *symtab,
                            char ***names, type_kind_t **types,
                            size_t **sizes, char ***tags,
                            int **restrict_flags,
                            size_t *count, int *is_variadic);
static int parse_func_body(parser_t *p, stmt_t ***body, size_t *count);

/* Parse '__attribute__((noreturn))' if present */
static int parse_gnu_noreturn(parser_t *p)
{
    size_t save = p->pos;
    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT || strcmp(tok->lexeme, "__attribute__") != 0)
        return 0;
    p->pos++;
    if (!match(p, TOK_LPAREN) || !match(p, TOK_LPAREN)) {
        p->pos = save; return 0;
    }
    tok = peek(p);
    if (!tok || tok->type != TOK_IDENT || strcmp(tok->lexeme, "noreturn") != 0) {
        p->pos = save; return 0;
    }
    p->pos++;
    if (!match(p, TOK_RPAREN) || !match(p, TOK_RPAREN)) {
        p->pos = save; return 0;
    }
    return 1;
}
/* Initialize the parser with a token array and reset position */
void parser_init(parser_t *p, token_t *tokens, size_t count)
{
    p->tokens = tokens;
    p->count = count;
    p->pos = 0;
}

/* Return non-zero once the parser has consumed all tokens */
int parser_is_eof(parser_t *p)
{
    token_t *tok = peek(p);
    return !tok || tok->type == TOK_EOF;
}

static size_t lookup_aggr_size(symtable_t *symtab, type_kind_t t,
                               const char *tag)
{
    if (!symtab || !tag)
        return 0;
    symbol_t *sym = NULL;
    if (t == TYPE_STRUCT)
        sym = symtable_lookup_struct(symtab, tag);
    else if (t == TYPE_UNION)
        sym = symtable_lookup_union(symtab, tag);
    if (!sym)
        return 0;
    return (t == TYPE_STRUCT) ? sym->struct_total_size : sym->total_size;
}



/* Print a parser error message showing the unexpected token */
void parser_print_error(parser_t *p, const token_type_t *expected,
                        size_t expected_count)
{
    token_t *tok = peek(p);
    char msg[256];
    size_t off;
    if (tok) {
        error_set(tok->line, tok->column, error_current_file, error_current_function);
        off = snprintf(msg, sizeof(msg), "Unexpected token '%s'", tok->lexeme);
    } else {
        error_set(0, 0, error_current_file, error_current_function);
        off = snprintf(msg, sizeof(msg), "Unexpected end of file");
    }

    if (expected_count > 0 && off < sizeof(msg)) {
        off += snprintf(msg + off, sizeof(msg) - off, ", expected ");
        for (size_t i = 0; i < expected_count && off < sizeof(msg); i++) {
            off += snprintf(msg + off, sizeof(msg) - off, "%s",
                            token_name(expected[i]));
            if (i + 1 < expected_count && off < sizeof(msg))
                off += snprintf(msg + off, sizeof(msg) - off, ", ");
        }
    }

    error_print(msg);
}

/* Free parameter vectors along with allocated tags */
static void cleanup_param_vectors(vector_t *names_v, vector_t *types_v,
                                  vector_t *sizes_v, vector_t *restrict_v,
                                  vector_t *tags_v)
{
    (void)names_v; /* names are borrowed from tokens */
    vector_free(names_v);
    vector_free(types_v);
    vector_free(sizes_v);
    vector_free(restrict_v);
    for (size_t i = 0; i < tags_v->count; i++)
        free(((char **)tags_v->data)[i]);
    vector_free(tags_v);
}

/* Parse a single parameter declaration and append info to vectors */
static int parse_param_decl(parser_t *p, symtable_t *symtab,
                            vector_t *names_v, vector_t *types_v,
                            vector_t *sizes_v, vector_t *tags_v,
                            vector_t *restrict_v)
{
    type_kind_t pt;
    char *tag = NULL;

    if (match(p, TOK_KW_STRUCT) || match(p, TOK_KW_UNION)) {
        token_type_t kw = p->tokens[p->pos - 1].type;
        token_t *id = peek(p);
        if (!id || id->type != TOK_IDENT)
            return 0;
        p->pos++;
        tag = vc_strdup(id->lexeme);
        if (!tag)
            return 0;
        pt = (kw == TOK_KW_STRUCT) ? TYPE_STRUCT : TYPE_UNION;
    } else if (!parse_basic_type(p, &pt)) {
        return 0;
    }

    size_t ps = (pt == TYPE_STRUCT || pt == TYPE_UNION)
                    ? lookup_aggr_size(symtab, pt, tag)
                    : basic_type_size(pt);

    int is_restrict = 0;
    if (match(p, TOK_STAR)) {
        pt = TYPE_PTR;
        is_restrict = match(p, TOK_KW_RESTRICT);
    }

    token_t *ptok = peek(p);
    if (!ptok || ptok->type != TOK_IDENT) {
        free(tag);
        return 0;
    }
    p->pos++;
    char *tmp_name = ptok->lexeme;

    if (!vector_push(names_v, &tmp_name) ||
        !vector_push(types_v, &pt) ||
        !vector_push(sizes_v, &ps) ||
        !vector_push(tags_v, &tag) ||
        !vector_push(restrict_v, &is_restrict)) {
        free(tag);
        return 0;
    }

    return 1;
}

/* Parse parameter list for a function definition. */
static int parse_param_list(parser_t *p, symtable_t *symtab,
                            char ***names, type_kind_t **types,
                            size_t **sizes, char ***tags,
                            int **restrict_flags,
                            size_t *count, int *is_variadic)
{
    vector_t names_v, types_v, sizes_v, restrict_v, tags_v;
    vector_init(&names_v, sizeof(char *));
    vector_init(&types_v, sizeof(type_kind_t));
    vector_init(&sizes_v, sizeof(size_t));
    vector_init(&restrict_v, sizeof(int));
    vector_init(&tags_v, sizeof(char *));
    *is_variadic = 0;

    if (!match(p, TOK_RPAREN)) {
        do {
            if (match(p, TOK_ELLIPSIS)) {
                *is_variadic = 1;
                break;
            }
            if (!parse_param_decl(p, symtab, &names_v, &types_v,
                                  &sizes_v, &tags_v, &restrict_v)) {
                cleanup_param_vectors(&names_v, &types_v,
                                     &sizes_v, &restrict_v, &tags_v);
                return 0;
            }
        } while (match(p, TOK_COMMA));
        if (!match(p, TOK_RPAREN)) {
            cleanup_param_vectors(&names_v, &types_v,
                                 &sizes_v, &restrict_v, &tags_v);
            return 0;
        }
    }

    *names = (char **)names_v.data;
    *types = (type_kind_t *)types_v.data;
    *sizes = (size_t *)sizes_v.data;
    *tags = (char **)tags_v.data;
    *restrict_flags = (int *)restrict_v.data;
    *count = names_v.count;
    return 1;
}

/* Parse the statements forming the function body. */
static int parse_func_body(parser_t *p, stmt_t ***body, size_t *count)
{
    vector_t body_v;
    vector_init(&body_v, sizeof(stmt_t *));

    while (!match(p, TOK_RBRACE)) {
        stmt_t *stmt = parser_parse_stmt(p);
        if (!stmt) {
            for (size_t i = 0; i < body_v.count; i++)
                ast_free_stmt(((stmt_t **)body_v.data)[i]);
            vector_free(&body_v);
            return 0;
        }
        if (!vector_push(&body_v, &stmt)) {
            ast_free_stmt(stmt);
            for (size_t i = 0; i < body_v.count; i++)
                ast_free_stmt(((stmt_t **)body_v.data)[i]);
            vector_free(&body_v);
            return 0;
        }
    }

    *body = (stmt_t **)body_v.data;
    *count = body_v.count;
    return 1;
}

/* Parse a full function definition */
func_t *parser_parse_func(parser_t *p, symtable_t *symtab,
                          int is_inline, int is_noreturn)
{
    type_kind_t ret_type;
    char *ret_tag = NULL;
    if (match(p, TOK_KW_STRUCT) || match(p, TOK_KW_UNION)) {
        token_type_t kw = p->tokens[p->pos - 1].type;
        token_t *id = peek(p);
        if (!id || id->type != TOK_IDENT)
            return NULL;
        p->pos++;
        ret_tag = vc_strdup(id->lexeme);
        if (!ret_tag)
            return NULL;
        ret_type = (kw == TOK_KW_STRUCT) ? TYPE_STRUCT : TYPE_UNION;
    } else if (!parse_basic_type(p, &ret_type)) {
        return NULL;
    }
    if (match(p, TOK_STAR)) {
        ret_type = TYPE_PTR;
        free(ret_tag); ret_tag = NULL;
    }

    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT)
        return NULL;
    p->pos++;
    char *name = tok->lexeme;

    if (!match(p, TOK_LPAREN))
        return NULL;

    char **param_names = NULL;
    type_kind_t *param_types = NULL;
    size_t *param_sizes = NULL;
    char **param_tags = NULL;
    int *param_restrict = NULL;
    size_t pcount = 0;
    int is_variadic = 0;
    if (!parse_param_list(p, symtab, &param_names, &param_types, &param_sizes,
                          &param_tags, &param_restrict, &pcount, &is_variadic))
        return NULL;

    if (parse_gnu_noreturn(p))
        is_noreturn = 1;

    if (!match(p, TOK_LBRACE)) {
        free(param_names);
        free(param_types);
        free(param_sizes);
        for (size_t i = 0; i < pcount; i++)
            free(param_tags[i]);
        free(param_tags);
        free(param_restrict);
        free(ret_tag);
        return NULL;
    }

    stmt_t **body = NULL;
    size_t count = 0;
    if (!parse_func_body(p, &body, &count)) {
        free(param_names);
        free(param_types);
        free(param_sizes);
        for (size_t i = 0; i < pcount; i++)
            free(param_tags[i]);
        free(param_tags);
        free(param_restrict);
        free(ret_tag);
        return NULL;
    }

    func_t *fn = ast_make_func(name, ret_type, ret_tag,
                               param_names, param_types,
                               param_tags,
                               param_sizes, param_restrict, pcount,
                               is_variadic,
                               body, count, is_inline, is_noreturn);
    if (!fn) {
        for (size_t i = 0; i < count; i++)
            ast_free_stmt(body[i]);
        free(body);
        free(param_names);
        free(param_types);
        free(param_sizes);
        for (size_t i = 0; i < pcount; i++)
            free(param_tags[i]);
        free(param_tags);
        free(param_restrict);
        free(ret_tag);
        return NULL;
    }
    free(ret_tag);
    return fn;
}

