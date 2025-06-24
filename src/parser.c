/*
 * Parser utilities and driver.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include "parser.h"
#include "vector.h"
#include "error.h"

void parser_init(parser_t *p, token_t *tokens, size_t count)
{
    p->tokens = tokens;
    p->count = count;
    p->pos = 0;
}

int parser_is_eof(parser_t *p)
{
    token_t *tok = peek(p);
    return !tok || tok->type == TOK_EOF;
}

static const char *token_name(token_type_t type)
{
    switch (type) {
    case TOK_EOF: return "end of file";
    case TOK_IDENT: return "identifier";
    case TOK_NUMBER: return "number";
    case TOK_STRING: return "string";
    case TOK_CHAR: return "character";
    case TOK_KW_INT: return "\"int\"";
    case TOK_KW_CHAR: return "\"char\"";
    case TOK_KW_VOID: return "\"void\"";
    case TOK_KW_RETURN: return "\"return\"";
    case TOK_KW_IF: return "\"if\"";
    case TOK_KW_ELSE: return "\"else\"";
    case TOK_KW_DO: return "\"do\"";
    case TOK_KW_WHILE: return "\"while\"";
    case TOK_KW_FOR: return "\"for\"";
    case TOK_KW_BREAK: return "\"break\"";
    case TOK_KW_CONTINUE: return "\"continue\"";
    case TOK_LPAREN: return "'('";
    case TOK_RPAREN: return "')'";
    case TOK_LBRACE: return "'{'";
    case TOK_RBRACE: return "'}'";
    case TOK_SEMI: return "';'";
    case TOK_COMMA: return "','";
    case TOK_PLUS: return "'+'";
    case TOK_MINUS: return "'-'";
    case TOK_AMP: return "'&'";
    case TOK_STAR: return "'*'";
    case TOK_SLASH: return "'/'";
    case TOK_ASSIGN: return "'='";
    case TOK_EQ: return "'=='";
    case TOK_NEQ: return "'!='";
    case TOK_LT: return "'<'";
    case TOK_GT: return "'>'";
    case TOK_LE: return "'<='";
    case TOK_GE: return "'>='";
    case TOK_LBRACKET: return "'['";
    case TOK_RBRACKET: return "']'";
    case TOK_UNKNOWN: return "unknown";
    }
    return "unknown";
}

void parser_print_error(parser_t *p, const token_type_t *expected,
                        size_t expected_count)
{
    token_t *tok = peek(p);
    char msg[256];
    size_t off;
    if (tok) {
        error_set(tok->line, tok->column);
        off = snprintf(msg, sizeof(msg), "Unexpected token '%s'", tok->lexeme);
    } else {
        error_set(0, 0);
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


func_t *parser_parse_func(parser_t *p)
{
    type_kind_t ret_type;
    if (match(p, TOK_KW_INT)) {
        ret_type = TYPE_INT;
    } else if (match(p, TOK_KW_CHAR)) {
        ret_type = TYPE_CHAR;
    } else if (match(p, TOK_KW_VOID)) {
        ret_type = TYPE_VOID;
    } else {
        return NULL;
    }

    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT)
        return NULL;
    p->pos++;
    char *name = tok->lexeme;

    if (!match(p, TOK_LPAREN))
        return NULL;

    vector_t param_names_v, param_types_v;
    vector_init(&param_names_v, sizeof(char *));
    vector_init(&param_types_v, sizeof(type_kind_t));

    if (!match(p, TOK_RPAREN)) {
        do {
            type_kind_t pt;
            if (match(p, TOK_KW_INT)) {
                pt = TYPE_INT;
                if (match(p, TOK_STAR))
                    pt = TYPE_PTR;
            } else if (match(p, TOK_KW_CHAR)) {
                pt = TYPE_CHAR;
                if (match(p, TOK_STAR))
                    pt = TYPE_PTR;
            } else {
                vector_free(&param_names_v);
                vector_free(&param_types_v);
                return NULL;
            }
            token_t *ptok = peek(p);
            if (!ptok || ptok->type != TOK_IDENT) {
                vector_free(&param_names_v);
                vector_free(&param_types_v);
                return NULL;
            }
            p->pos++;
            char *tmp_name = ptok->lexeme;
            if (!vector_push(&param_names_v, &tmp_name) ||
                !vector_push(&param_types_v, &pt)) {
                vector_free(&param_names_v);
                vector_free(&param_types_v);
                return NULL;
            }
        } while (match(p, TOK_COMMA));
        if (!match(p, TOK_RPAREN)) {
            vector_free(&param_names_v);
            vector_free(&param_types_v);
            return NULL;
        }
    }

    if (!match(p, TOK_LBRACE)) {
        vector_free(&param_names_v);
        vector_free(&param_types_v);
        return NULL;
    }

    vector_t body_v;
    vector_init(&body_v, sizeof(stmt_t *));

    while (!match(p, TOK_RBRACE)) {
        stmt_t *stmt = parser_parse_stmt(p);
        if (!stmt) {
            for (size_t i = 0; i < body_v.count; i++)
                ast_free_stmt(((stmt_t **)body_v.data)[i]);
            vector_free(&body_v);
            vector_free(&param_names_v);
            vector_free(&param_types_v);
            return NULL;
        }
        if (!vector_push(&body_v, &stmt)) {
            ast_free_stmt(stmt);
            for (size_t i = 0; i < body_v.count; i++)
                ast_free_stmt(((stmt_t **)body_v.data)[i]);
            vector_free(&body_v);
            vector_free(&param_names_v);
            vector_free(&param_types_v);
            return NULL;
        }
    }

    char **param_names = (char **)param_names_v.data;
    type_kind_t *param_types = (type_kind_t *)param_types_v.data;
    size_t pcount = param_names_v.count;
    stmt_t **body = (stmt_t **)body_v.data;
    size_t count = body_v.count;

    func_t *fn = ast_make_func(name, ret_type,
                               param_names, param_types, pcount,
                               body, count);
    if (!fn) {
        for (size_t i = 0; i < count; i++)
            ast_free_stmt(body[i]);
        free(body);
        free(param_names);
        free(param_types);
        return NULL;
    }
    return fn;
}

int parser_parse_toplevel(parser_t *p, func_t **out_func, stmt_t **out_global)
{
    if (out_func) *out_func = NULL;
    if (out_global) *out_global = NULL;

    size_t save = p->pos;
    token_t *tok = peek(p);
    if (!tok)
        return 0;

    type_kind_t t;
    if (tok->type == TOK_KW_INT) {
        t = TYPE_INT;
    } else if (tok->type == TOK_KW_CHAR) {
        t = TYPE_CHAR;
    } else if (tok->type == TOK_KW_VOID) {
        t = TYPE_VOID;
    } else {
        return 0;
    }
    p->pos++;
    if ((t == TYPE_INT || t == TYPE_CHAR) && match(p, TOK_STAR))
        t = TYPE_PTR;

    token_t *id = peek(p);
    if (!id || id->type != TOK_IDENT) {
        p->pos = save;
        return 0;
    }
    p->pos++;

    size_t arr_size = 0;
    token_t *next_tok;
    if ((next_tok = peek(p)) && next_tok->type == TOK_LBRACKET) {
        p->pos++; /* '[' */
        token_t *num = peek(p);
        if (!num || num->type != TOK_NUMBER) {
            p->pos = save;
            return 0;
        }
        p->pos++;
        arr_size = strtoul(num->lexeme, NULL, 10);
        if (!match(p, TOK_RBRACKET)) {
            p->pos = save;
            return 0;
        }
        t = TYPE_ARRAY;
    }

    token_t *next = peek(p);
    if (next && next->type == TOK_SEMI) {
        if (t == TYPE_VOID) {
            p->pos = save;
            return 0;
        }
        p->pos++; /* consume ';' */
        if (out_global)
            *out_global = ast_make_var_decl(id->lexeme, t, arr_size, NULL,
                                           NULL, 0, tok->line, tok->column);
        return *out_global != NULL;
    } else if (next && next->type == TOK_ASSIGN) {
        if (t == TYPE_VOID) {
            p->pos = save;
            return 0;
        }
        p->pos++; /* consume '=' */
        expr_t *init = NULL;
        expr_t **init_list = NULL;
        size_t init_count = 0;
        if (t == TYPE_ARRAY && peek(p) && peek(p)->type == TOK_LBRACE) {
            init_list = parser_parse_init_list(p, &init_count);
            if (!init_list || !match(p, TOK_SEMI)) {
                if (init_list) {
                    for (size_t i = 0; i < init_count; i++)
                        ast_free_expr(init_list[i]);
                    free(init_list);
                }
                p->pos = save;
                return 0;
            }
        } else {
            init = parser_parse_expr(p);
            if (!init || !match(p, TOK_SEMI)) {
                ast_free_expr(init);
                p->pos = save;
                return 0;
            }
        }
        if (out_global)
            *out_global = ast_make_var_decl(id->lexeme, t, arr_size, init,
                                           init_list, init_count,
                                           tok->line, tok->column);
        return *out_global != NULL;
    }

    p->pos = save;
    if (out_func)
        *out_func = parser_parse_func(p);
    return *out_func != NULL;
}
