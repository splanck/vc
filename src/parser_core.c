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

/* Helper prototypes */
static int parse_param_list(parser_t *p,
                            char ***names, type_kind_t **types,
                            size_t **sizes, int **restrict_flags,
                            size_t *count, int *is_variadic);
static int parse_func_body(parser_t *p, stmt_t ***body, size_t *count);
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

/* Lookup table mapping token_type_t values to textual names used in
 * diagnostics.  The array indices correspond to the enumeration
 * constants defined in ``token.h''.  Keep this table in sync with the
 * token_type_t enum when adding new tokens. */
static const char *token_names[] = {
    [TOK_EOF] = "end of file",
    [TOK_IDENT] = "identifier",
    [TOK_NUMBER] = "number",
    [TOK_STRING] = "string",
    [TOK_CHAR] = "character",
    [TOK_WIDE_STRING] = "L\"string\"",
    [TOK_WIDE_CHAR] = "L'char'",
    [TOK_KW_INT] = "\"int\"",
    [TOK_KW_CHAR] = "\"char\"",
    [TOK_KW_FLOAT] = "\"float\"",
    [TOK_KW_DOUBLE] = "\"double\"",
    [TOK_KW_SHORT] = "\"short\"",
    [TOK_KW_LONG] = "\"long\"",
    [TOK_KW_BOOL] = "\"bool\"",
    [TOK_KW_UNSIGNED] = "\"unsigned\"",
    [TOK_KW_VOID] = "\"void\"",
    [TOK_KW_ENUM] = "\"enum\"",
    [TOK_KW_STRUCT] = "\"struct\"",
    [TOK_KW_UNION] = "\"union\"",
    [TOK_KW_TYPEDEF] = "\"typedef\"",
    [TOK_KW_STATIC] = "\"static\"",
    [TOK_KW_EXTERN] = "\"extern\"",
    [TOK_KW_CONST] = "\"const\"",
    [TOK_KW_VOLATILE] = "\"volatile\"",
    [TOK_KW_RESTRICT] = "\"restrict\"",
    [TOK_KW_REGISTER] = "\"register\"",
    [TOK_KW_INLINE] = "\"inline\"",
    [TOK_KW_RETURN] = "\"return\"",
    [TOK_KW_IF] = "\"if\"",
    [TOK_KW_ELSE] = "\"else\"",
    [TOK_KW_DO] = "\"do\"",
    [TOK_KW_WHILE] = "\"while\"",
    [TOK_KW_FOR] = "\"for\"",
    [TOK_KW_BREAK] = "\"break\"",
    [TOK_KW_CONTINUE] = "\"continue\"",
    [TOK_KW_GOTO] = "\"goto\"",
    [TOK_KW_SWITCH] = "\"switch\"",
    [TOK_KW_CASE] = "\"case\"",
    [TOK_KW_DEFAULT] = "\"default\"",
    [TOK_KW_SIZEOF] = "\"sizeof\"",
    [TOK_LPAREN] = "'('",
    [TOK_RPAREN] = ")",
    [TOK_LBRACE] = "'{'",
    [TOK_RBRACE] = "'}'",
    [TOK_SEMI] = ";",
    [TOK_COMMA] = ",",
    [TOK_PLUS] = "+",
    [TOK_MINUS] = "-",
    [TOK_DOT] = ".",
    [TOK_ARROW] = "'->'",
    [TOK_AMP] = "&",
    [TOK_STAR] = "*",
    [TOK_SLASH] = "/",
    [TOK_PERCENT] = "%",
    [TOK_PIPE] = "|",
    [TOK_CARET] = "^",
    [TOK_SHL] = "'<<'",
    [TOK_SHR] = "'>>'",
    [TOK_PLUSEQ] = "+=",
    [TOK_MINUSEQ] = "-=",
    [TOK_STAREQ] = "*=",
    [TOK_SLASHEQ] = "/=",
    [TOK_PERCENTEQ] = "%=",
    [TOK_AMPEQ] = "&=",
    [TOK_PIPEEQ] = "|=",
    [TOK_CARETEQ] = "^=",
    [TOK_SHLEQ] = "<<=",
    [TOK_SHREQ] = ">>=",
    [TOK_INC] = "++",
    [TOK_DEC] = "--",
    [TOK_ASSIGN] = "=",
    [TOK_EQ] = "==",
    [TOK_NEQ] = "!=",
    [TOK_LOGAND] = "&&",
    [TOK_LOGOR] = "||",
    [TOK_NOT] = "!",
    [TOK_LT] = "<",
    [TOK_GT] = ">",
    [TOK_LE] = "<=",
    [TOK_GE] = ">=",
    [TOK_LBRACKET] = "[",
    [TOK_RBRACKET] = "]",
    [TOK_QMARK] = "?",
    [TOK_COLON] = ":",
    [TOK_LABEL] = "label",
    [TOK_ELLIPSIS] = "'...'",
    [TOK_UNKNOWN] = "unknown"
};

/* Map a token type to a human readable name used in error messages */
static const char *token_name(token_type_t type)
{
    size_t n = sizeof(token_names) / sizeof(token_names[0]);
    if (type >= 0 && (size_t)type < n && token_names[type])
        return token_names[type];
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

/* Parse parameter list for a function definition. */
static int parse_param_list(parser_t *p,
                            char ***names, type_kind_t **types,
                            size_t **sizes, int **restrict_flags,
                            size_t *count, int *is_variadic)
{
    vector_t names_v, types_v, sizes_v, restrict_v;
    vector_init(&names_v, sizeof(char *));
    vector_init(&types_v, sizeof(type_kind_t));
    vector_init(&sizes_v, sizeof(size_t));
    vector_init(&restrict_v, sizeof(int));
    *is_variadic = 0;

    if (!match(p, TOK_RPAREN)) {
        do {
            if (match(p, TOK_ELLIPSIS)) {
                *is_variadic = 1;
                break;
            }
            type_kind_t pt;
            if (!parse_basic_type(p, &pt)) {
                vector_free(&names_v);
                vector_free(&types_v);
                vector_free(&sizes_v);
                vector_free(&restrict_v);
                return 0;
            }
            size_t ps = basic_type_size(pt);
            int is_restrict = 0;
            if (match(p, TOK_STAR)) {
                pt = TYPE_PTR;
                is_restrict = match(p, TOK_KW_RESTRICT);
            }
            token_t *ptok = peek(p);
            if (!ptok || ptok->type != TOK_IDENT) {
                vector_free(&names_v);
                vector_free(&types_v);
                vector_free(&sizes_v);
                vector_free(&restrict_v);
                return 0;
            }
            p->pos++;
            char *tmp_name = ptok->lexeme;
            if (!vector_push(&names_v, &tmp_name) ||
                !vector_push(&types_v, &pt) ||
                !vector_push(&sizes_v, &ps) ||
                !vector_push(&restrict_v, &is_restrict)) {
                vector_free(&names_v);
                vector_free(&types_v);
                vector_free(&sizes_v);
                vector_free(&restrict_v);
                return 0;
            }
        } while (match(p, TOK_COMMA));
        if (!match(p, TOK_RPAREN)) {
            vector_free(&names_v);
            vector_free(&types_v);
            vector_free(&sizes_v);
            vector_free(&restrict_v);
            return 0;
        }
    }

    *names = (char **)names_v.data;
    *types = (type_kind_t *)types_v.data;
    *sizes = (size_t *)sizes_v.data;
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
func_t *parser_parse_func(parser_t *p, int is_inline)
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

    char **param_names = NULL;
    type_kind_t *param_types = NULL;
    size_t *param_sizes = NULL;
    int *param_restrict = NULL;
    size_t pcount = 0;
    int is_variadic = 0;
    if (!parse_param_list(p, &param_names, &param_types, &param_sizes,
                          &param_restrict, &pcount, &is_variadic))
        return NULL;

    if (!match(p, TOK_LBRACE)) {
        free(param_names);
        free(param_types);
        free(param_sizes);
        free(param_restrict);
        return NULL;
    }

    stmt_t **body = NULL;
    size_t count = 0;
    if (!parse_func_body(p, &body, &count)) {
        free(param_names);
        free(param_types);
        free(param_sizes);
        free(param_restrict);
        return NULL;
    }

    func_t *fn = ast_make_func(name, ret_type,
                               param_names, param_types,
                               param_sizes, param_restrict, pcount,
                               is_variadic,
                               body, count, is_inline);
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

