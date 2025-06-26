/*
 * Parser utilities and driver.
 *
 * The parser consumes the token stream using a simple recursive
 * descent approach.  "parser_parse_toplevel" is the entry point that
 * recognizes either a function definition or a global variable
 * declaration.  Expressions and statements are handled in the helper
 * modules parser_expr.c and parser_stmt.c respectively.
 *
 * Each function advances "p->pos" on success and returns a newly
 * allocated AST node (or 1 for boolean functions).  A NULL return value
 * indicates a syntax error and the caller is responsible for error
 * reporting.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include "parser.h"
#include "vector.h"
#include "error.h"
#include "parser_types.h"

/*
 * Initialize parser state with the provided token array.  "p->pos" is
 * reset to the beginning of the stream.
 */
void parser_init(parser_t *p, token_t *tokens, size_t count)
{
    p->tokens = tokens;
    p->count = count;
    p->pos = 0;
}

/* Return non-zero once the parser has consumed all tokens. */
int parser_is_eof(parser_t *p)
{
    token_t *tok = peek(p);
    return !tok || tok->type == TOK_EOF;
}

/* Map a token type to a human readable name used in error messages. */
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
    case TOK_KW_CONST: return "\"const\"";
    case TOK_KW_VOLATILE: return "\"volatile\"";
    case TOK_KW_RESTRICT: return "\"restrict\"";
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
    case TOK_RPAREN: return "')'";
    case TOK_LBRACE: return "'{'";
    case TOK_RBRACE: return "'}'";
    case TOK_SEMI: return "';'";
    case TOK_COMMA: return "','";
    case TOK_PLUS: return "'+'";
    case TOK_MINUS: return "'-'";
    case TOK_DOT: return "'.'";
    case TOK_ARROW: return "'->'";
    case TOK_AMP: return "'&'";
    case TOK_PIPE: return "'|'";
    case TOK_CARET: return "'^'";
    case TOK_SHL: return "'<<'";
    case TOK_SHR: return "'>>'";
    case TOK_STAR: return "'*'";
    case TOK_SLASH: return "'/'";
    case TOK_PERCENT: return "'%'";
    case TOK_PLUSEQ: return "'+='";
    case TOK_MINUSEQ: return "'-='";
    case TOK_STAREQ: return "'*='";
    case TOK_SLASHEQ: return "'/='";
    case TOK_PERCENTEQ: return "'%='";
    case TOK_AMPEQ: return "'&='";
    case TOK_PIPEEQ: return "'|='";
    case TOK_CARETEQ: return "'^='";
    case TOK_SHLEQ: return "'<<='";
    case TOK_SHREQ: return "'>>='";
    case TOK_INC: return "'++'";
    case TOK_DEC: return "'--'";
    case TOK_ASSIGN: return "'='";
    case TOK_EQ: return "'=='";
    case TOK_NEQ: return "'!='";
    case TOK_LOGAND: return "'&&'";
    case TOK_LOGOR: return "'||'";
    case TOK_NOT: return "'!'";
    case TOK_LT: return "'<'";
    case TOK_GT: return "'>'";
    case TOK_LE: return "'<='";
    case TOK_GE: return "'>='";
    case TOK_LBRACKET: return "'['";
    case TOK_RBRACKET: return "']'";
    case TOK_QMARK: return "'?'";
    case TOK_COLON: return "':'";
    case TOK_LABEL: return "label";
    case TOK_UNKNOWN: return "unknown";
    }
    return "unknown";
}



/*
 * Emit an error at the current token.  "expected" is an optional list of
 * token kinds that would have been valid in this context.
 */
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

/*
 * Parse a full function definition of the form:
 *     <type> <name>(<params>) { <body> }
 * On success a new func_t is returned and the parser position is
 * updated to the token following the closing brace.
 */
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

/*
 * Parse either a global variable declaration or a full function
 * definition.  Exactly one of "out_func" or "out_global" is set on
 * success.  The function returns 1 when a valid top-level construct was
 * consumed and 0 otherwise.
 */
int parser_parse_toplevel(parser_t *p, symtable_t *funcs,
                          func_t **out_func, stmt_t **out_global)
{
    if (out_func) *out_func = NULL;
    if (out_global) *out_global = NULL;

    size_t save = p->pos;
    int is_static = match(p, TOK_KW_STATIC);
    int is_const = match(p, TOK_KW_CONST);
    int is_volatile = match(p, TOK_KW_VOLATILE);
    token_t *tok = peek(p);
    if (!tok)
        return 0;

    if (tok->type == TOK_KW_STRUCT) {
        token_t *next = &p->tokens[p->pos + 1];
        if (next && next->type == TOK_IDENT &&
            p->pos + 2 < p->count && p->tokens[p->pos + 2].type == TOK_LBRACE) {
            p->pos = save;
            if (out_global)
                *out_global = parser_parse_struct_decl(p);
            else
                parser_parse_struct_decl(p);
            return out_global ? *out_global != NULL : 1;
        }
        p->pos = save;
        if (out_global)
            *out_global = parser_parse_struct_var_decl(p);
        else
            parser_parse_struct_var_decl(p);
        return out_global ? *out_global != NULL : 1;
    }

    if (tok->type == TOK_KW_UNION) {
        token_t *next = &p->tokens[p->pos + 1];
        if (next && next->type == TOK_IDENT &&
            p->pos + 2 < p->count && p->tokens[p->pos + 2].type == TOK_LBRACE) {
            p->pos = save;
            if (out_global)
                *out_global = parser_parse_union_decl(p);
            else
                parser_parse_union_decl(p);
            return out_global ? *out_global != NULL : 1;
        }
        p->pos = save;
        if (out_global)
            *out_global = parser_parse_union_var_decl(p);
        else
            parser_parse_union_var_decl(p);
        return out_global ? *out_global != NULL : 1;
    }

    if (tok->type == TOK_KW_ENUM) {
        p->pos++;
        stmt_t *decl = parser_parse_enum_decl(p);
        if (!decl) {
            p->pos = save;
            return 0;
        }
        if (out_global)
            *out_global = decl;
        return 1;
    }

    if (tok->type == TOK_KW_TYPEDEF) {
        p->pos++;
        type_kind_t tt;
        if (!parse_basic_type(p, &tt)) { p->pos = save; return 0; }
        size_t elem_sz = basic_type_size(tt);
        if (match(p, TOK_STAR))
            tt = TYPE_PTR;
        token_t *name_tok = peek(p);
        if (!name_tok || name_tok->type != TOK_IDENT) { p->pos = save; return 0; }
        p->pos++;
        size_t arr_size = 0;
        if (match(p, TOK_LBRACKET)) {
            token_t *num = peek(p);
            if (!num || num->type != TOK_NUMBER) { p->pos = save; return 0; }
            p->pos++;
            arr_size = strtoul(num->lexeme, NULL, 10);
            if (!match(p, TOK_RBRACKET)) { p->pos = save; return 0; }
            tt = TYPE_ARRAY;
        }
        if (!match(p, TOK_SEMI)) { p->pos = save; return 0; }
        if (out_global)
            *out_global = ast_make_typedef(name_tok->lexeme, tt, arr_size,
                                          elem_sz, tok->line, tok->column);
        return *out_global != NULL;
    }

    type_kind_t t;
    if (!parse_basic_type(p, &t))
        return 0;
    size_t elem_size = basic_type_size(t);
    int is_restrict = 0;
    if (match(p, TOK_STAR)) {
        t = TYPE_PTR;
        is_restrict = match(p, TOK_KW_RESTRICT);
    }

    token_t *id = peek(p);
    if (!id || id->type != TOK_IDENT) {
        p->pos = save;
        return 0;
    }
    p->pos++;

    size_t arr_size = 0;
    token_t *next_tok = peek(p);
    if (next_tok && next_tok->type == TOK_LPAREN) {
        /* lookahead for prototype or definition */
        p->pos++; /* '(' */
        vector_t param_types_v;
        vector_init(&param_types_v, sizeof(type_kind_t));
        if (!match(p, TOK_RPAREN)) {
            do {
                type_kind_t pt;
                if (!parse_basic_type(p, &pt)) {
                    vector_free(&param_types_v);
                    p->pos = save;
                    return 0;
                }
                if (match(p, TOK_STAR)) {
                    pt = TYPE_PTR;
                    match(p, TOK_KW_RESTRICT);
                }
                token_t *tmp = peek(p);
                if (tmp && tmp->type == TOK_IDENT)
                    p->pos++; /* optional name */
                if (!vector_push(&param_types_v, &pt)) {
                    vector_free(&param_types_v);
                    p->pos = save;
                    return 0;
                }
            } while (match(p, TOK_COMMA));
            if (!match(p, TOK_RPAREN)) {
                vector_free(&param_types_v);
                p->pos = save;
                return 0;
            }
        }
        token_t *after = peek(p);
        if (after && after->type == TOK_SEMI) {
            p->pos++; /* ';' */
            symtable_add_func(funcs, id->lexeme, t,
                             (type_kind_t *)param_types_v.data,
                             param_types_v.count, 1);
            vector_free(&param_types_v);
            return 1;
        } else if (after && after->type == TOK_LBRACE) {
            vector_free(&param_types_v);
            p->pos = save;
            if (out_func)
                *out_func = parser_parse_func(p);
            return *out_func != NULL;
        } else {
            vector_free(&param_types_v);
            p->pos = save;
            return 0;
        }
    }

    expr_t *size_expr = NULL;
    if (next_tok && next_tok->type == TOK_LBRACKET) {
        p->pos++; /* '[' */
        size_expr = parser_parse_expr(p);
        if (!size_expr || !match(p, TOK_RBRACKET)) {
            ast_free_expr(size_expr);
            p->pos = save;
            return 0;
        }
        if (size_expr->kind == EXPR_NUMBER)
            arr_size = strtoul(size_expr->number.value, NULL, 10);
        t = TYPE_ARRAY;
        next_tok = peek(p);
    }

    if (next_tok && next_tok->type == TOK_SEMI) {
        if (t == TYPE_VOID) {
            p->pos = save;
            return 0;
        }
        p->pos++; /* consume ';' */
        if (out_global)
            *out_global = ast_make_var_decl(id->lexeme, t, arr_size, size_expr,
                                           elem_size, is_static, is_const,
                                           is_volatile, is_restrict,
                                           NULL, NULL, 0,
                                           NULL, NULL, 0,
                                           tok->line, tok->column);
        return *out_global != NULL;
    } else if (next_tok && next_tok->type == TOK_ASSIGN) {
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
            *out_global = ast_make_var_decl(id->lexeme, t, arr_size, size_expr,
                                           elem_size, is_static, is_const,
                                           is_volatile, is_restrict,
                                           init, init_list, init_count,
                                           NULL, NULL, 0,
                                           tok->line, tok->column);
        return *out_global != NULL;
    }

    p->pos = save;
    if (out_func)
        *out_func = parser_parse_func(p);
    return *out_func != NULL;
}
