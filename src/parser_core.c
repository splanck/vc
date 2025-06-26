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

/* Map a token type to a human readable name used in error messages */
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
    case TOK_KW_FLOAT: return "\"float\"";
    case TOK_KW_DOUBLE: return "\"double\"";
    case TOK_KW_SHORT: return "\"short\"";
    case TOK_KW_LONG: return "\"long\"";
    case TOK_KW_BOOL: return "\"bool\"";
    case TOK_KW_UNSIGNED: return "\"unsigned\"";
    case TOK_KW_VOID: return "\"void\"";
    case TOK_KW_ENUM: return "\"enum\"";
    case TOK_KW_STRUCT: return "\"struct\"";
    case TOK_KW_UNION: return "\"union\"";
    case TOK_KW_TYPEDEF: return "\"typedef\"";
    case TOK_KW_STATIC: return "\"static\"";
    case TOK_KW_EXTERN: return "\"extern\"";
    case TOK_KW_CONST: return "\"const\"";
    case TOK_KW_VOLATILE: return "\"volatile\"";
    case TOK_KW_RESTRICT: return "\"restrict\"";
    case TOK_KW_REGISTER: return "\"register\"";
    case TOK_KW_INLINE: return "\"inline\"";
    case TOK_KW_RETURN: return "\"return\"";
    case TOK_KW_IF: return "\"if\"";
    case TOK_KW_ELSE: return "\"else\"";
    case TOK_KW_DO: return "\"do\"";
    case TOK_KW_WHILE: return "\"while\"";
    case TOK_KW_FOR: return "\"for\"";
    case TOK_KW_BREAK: return "\"break\"";
    case TOK_KW_CONTINUE: return "\"continue\"";
    case TOK_KW_GOTO: return "\"goto\"";
    case TOK_KW_SWITCH: return "\"switch\"";
    case TOK_KW_CASE: return "\"case\"";
    case TOK_KW_DEFAULT: return "\"default\"";
    case TOK_KW_SIZEOF: return "\"sizeof\"";
    case TOK_LPAREN: return "'('";
    case TOK_RPAREN: return ")";
    case TOK_LBRACE: return "'{'";
    case TOK_RBRACE: return "'}'";
    case TOK_SEMI: return ";";
    case TOK_COMMA: return ",";
    case TOK_PLUS: return "+";
    case TOK_MINUS: return "-";
    case TOK_DOT: return ".";
    case TOK_ARROW: return "'->'";
    case TOK_AMP: return "&";
    case TOK_PIPE: return "|";
    case TOK_CARET: return "^";
    case TOK_SHL: return "'<<'";
    case TOK_SHR: return "'>>'";
    case TOK_STAR: return "*";
    case TOK_SLASH: return "/";
    case TOK_PERCENT: return "%";
    case TOK_PLUSEQ: return "+=";
    case TOK_MINUSEQ: return "-=";
    case TOK_STAREQ: return "*=";
    case TOK_SLASHEQ: return "/=";
    case TOK_PERCENTEQ: return "%=";
    case TOK_AMPEQ: return "&=";
    case TOK_PIPEEQ: return "|=";
    case TOK_CARETEQ: return "^=";
    case TOK_SHLEQ: return "<<=";
    case TOK_SHREQ: return ">>=";
    case TOK_INC: return "++";
    case TOK_DEC: return "--";
    case TOK_ASSIGN: return "=";
    case TOK_EQ: return "==";
    case TOK_NEQ: return "!=";
    case TOK_LOGAND: return "&&";
    case TOK_LOGOR: return "||";
    case TOK_NOT: return "!";
    case TOK_LT: return "<";
    case TOK_GT: return ">";
    case TOK_LE: return "<=";
    case TOK_GE: return ">=";
    case TOK_LBRACKET: return "[";
    case TOK_RBRACKET: return "]";
    case TOK_QMARK: return "?";
    case TOK_COLON: return ":";
    case TOK_LABEL: return "label";
    case TOK_UNKNOWN: return "unknown";
    }
    return "unknown";
}

/* Print a parser error message showing the unexpected token */
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

/* Parse a full function definition */
func_t *parser_parse_func(parser_t *p)
{
    type_kind_t ret_type;
    if (!parse_basic_type(p, &ret_type))
        return NULL;
    if (match(p, TOK_STAR))
        ret_type = TYPE_PTR;

    token_t *tok = peek(p);
    if (!tok || tok->type != TOK_IDENT)
        return NULL;
    p->pos++;
    char *name = tok->lexeme;

    if (!match(p, TOK_LPAREN))
        return NULL;

    vector_t param_names_v, param_types_v, param_sizes_v, param_restrict_v;
    vector_init(&param_names_v, sizeof(char *));
    vector_init(&param_types_v, sizeof(type_kind_t));
    vector_init(&param_sizes_v, sizeof(size_t));
    vector_init(&param_restrict_v, sizeof(int));

    if (!match(p, TOK_RPAREN)) {
        do {
            type_kind_t pt;
            if (!parse_basic_type(p, &pt)) {
                vector_free(&param_names_v);
                vector_free(&param_types_v);
                vector_free(&param_sizes_v);
                vector_free(&param_restrict_v);
                return NULL;
            }
            size_t ps = basic_type_size(pt);
            int is_restrict = 0;
            if (match(p, TOK_STAR)) {
                pt = TYPE_PTR;
                is_restrict = match(p, TOK_KW_RESTRICT);
            }
            token_t *ptok = peek(p);
            if (!ptok || ptok->type != TOK_IDENT) {
                vector_free(&param_names_v);
                vector_free(&param_types_v);
                vector_free(&param_sizes_v);
                vector_free(&param_restrict_v);
                return NULL;
            }
            p->pos++;
            char *tmp_name = ptok->lexeme;
            if (!vector_push(&param_names_v, &tmp_name) ||
                !vector_push(&param_types_v, &pt) ||
                !vector_push(&param_sizes_v, &ps) ||
                !vector_push(&param_restrict_v, &is_restrict)) {
                vector_free(&param_names_v);
                vector_free(&param_types_v);
                vector_free(&param_sizes_v);
                vector_free(&param_restrict_v);
                return NULL;
            }
        } while (match(p, TOK_COMMA));
        if (!match(p, TOK_RPAREN)) {
            vector_free(&param_names_v);
            vector_free(&param_types_v);
            vector_free(&param_sizes_v);
            return NULL;
        }
    }

    if (!match(p, TOK_LBRACE)) {
        vector_free(&param_names_v);
        vector_free(&param_types_v);
        vector_free(&param_sizes_v);
        vector_free(&param_restrict_v);
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
            vector_free(&param_sizes_v);
            vector_free(&param_restrict_v);
            return NULL;
        }
        if (!vector_push(&body_v, &stmt)) {
            ast_free_stmt(stmt);
            for (size_t i = 0; i < body_v.count; i++)
                ast_free_stmt(((stmt_t **)body_v.data)[i]);
            vector_free(&body_v);
            vector_free(&param_names_v);
            vector_free(&param_types_v);
            vector_free(&param_sizes_v);
            vector_free(&param_restrict_v);
            return NULL;
        }
    }

    char **param_names = (char **)param_names_v.data;
    type_kind_t *param_types = (type_kind_t *)param_types_v.data;
    size_t *param_sizes = (size_t *)param_sizes_v.data;
    int *param_restrict = (int *)param_restrict_v.data;
    size_t pcount = param_names_v.count;
    stmt_t **body = (stmt_t **)body_v.data;
    size_t count = body_v.count;

    func_t *fn = ast_make_func(name, ret_type,
                               param_names, param_types,
                               param_sizes, param_restrict, pcount,
                               body, count);
    if (!fn) {
        for (size_t i = 0; i < count; i++)
            ast_free_stmt(body[i]);
        free(body);
        free(param_names);
        free(param_types);
        free(param_sizes);
        free(param_restrict);
        return NULL;
    }
    return fn;
}

