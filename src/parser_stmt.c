/*
 * Statement parser for the language.
 *
 * This module acts as a thin dispatcher for statement parsing.  It
 * recognizes the statement kind and forwards to the appropriate helper
 * in parser_decl.c or parser_flow.c.  Only basic block and simple
 * statements are handled directly here.  Declaration parsing is
 * performed by small helper routines that leave the parser state
 * unchanged on failure so callers can attempt multiple forms.
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

/* Forward declarations for control flow helpers */
stmt_t *parser_parse_if_stmt(parser_t *p);
stmt_t *parser_parse_while_stmt(parser_t *p);
stmt_t *parser_parse_do_while_stmt(parser_t *p);
stmt_t *parser_parse_for_stmt(parser_t *p);
stmt_t *parser_parse_switch_stmt(parser_t *p);

static stmt_t *parse_block(parser_t *p);
static stmt_t *parse_tagged_decl(parser_t *p, token_type_t keyword,
                                 stmt_t *(*decl_fn)(parser_t *),
                                 stmt_t *(*var_decl_fn)(parser_t *));
static stmt_t *parse_enum_declaration(parser_t *p);
stmt_t *parser_parse_static_assert(parser_t *p);
static stmt_t *parse_struct_declaration(parser_t *p);
static stmt_t *parse_union_declaration(parser_t *p);
static stmt_t *maybe_parse_var_decl(parser_t *p);
static stmt_t *parse_declaration_stmt(parser_t *p);
static stmt_t *parse_jump_stmt(parser_t *p);
static stmt_t *parse_flow_stmt(parser_t *p);
stmt_t *parser_parse_stmt(parser_t *p);

/* Parse a "{...}" block recursively collecting inner statements. */
static stmt_t *parse_block(parser_t *p)
{
    if (!match(p, TOK_LBRACE))
        return NULL;
    token_t *lb_tok = &p->tokens[p->pos - 1];
    vector_t stmts_v;
    vector_init(&stmts_v, sizeof(stmt_t *));
    while (!match(p, TOK_RBRACE)) {
        stmt_t *s = parser_parse_stmt(p);
        if (!s) {
            for (size_t i = 0; i < stmts_v.count; i++)
                ast_free_stmt(((stmt_t **)stmts_v.data)[i]);
            vector_free(&stmts_v);
            return NULL;
        }
        if (!vector_push(&stmts_v, &s)) {
            ast_free_stmt(s);
            for (size_t i = 0; i < stmts_v.count; i++)
                ast_free_stmt(((stmt_t **)stmts_v.data)[i]);
            vector_free(&stmts_v);
            return NULL;
        }
    }
    stmt_t **stmts = (stmt_t **)stmts_v.data;
    size_t count = stmts_v.count;
    return ast_make_block(stmts, count, lb_tok->line, lb_tok->column);
}

/* Common helper for enum/struct/union declarations. */
static stmt_t *parse_tagged_decl(parser_t *p, token_type_t keyword,
                                 stmt_t *(*decl_fn)(parser_t *),
                                 stmt_t *(*var_decl_fn)(parser_t *))
{
    size_t save = p->pos;
    int has_static = match(p, TOK_KW_STATIC);
    int has_reg = match(p, TOK_KW_REGISTER);
    int has_const = match(p, TOK_KW_CONST);
    int has_vol = match(p, TOK_KW_VOLATILE);

    if (!match(p, keyword)) {
        p->pos = save;
        return NULL;
    }

    token_t *next = peek(p);
    if (next && next->type == TOK_LBRACE) {
        p->pos = save;
        return var_decl_fn(p);
    } else if (next && next->type == TOK_IDENT) {
        p->pos++;
        token_t *after = peek(p);
        if (!has_static && !has_reg && !has_const && !has_vol && after &&
            after->type == TOK_LBRACE) {
            p->pos = save;
            return decl_fn(p);
        }
        p->pos = save;
        return parser_parse_var_decl(p);
    }

    p->pos = save;
    return NULL;
}

/* Parse an enum declaration or inline enum variable definition. */
static stmt_t *parse_enum_declaration(parser_t *p)
{
    return parse_tagged_decl(p, TOK_KW_ENUM,
                             parser_parse_enum_decl,
                             parser_parse_enum_decl);
}

/* Parse a struct declaration or inline struct variable definition. */
static stmt_t *parse_struct_declaration(parser_t *p)
{
    return parse_tagged_decl(p, TOK_KW_STRUCT,
                             parser_parse_struct_decl,
                             parser_parse_struct_var_decl);
}

/* Parse a union declaration or inline union variable definition. */
static stmt_t *parse_union_declaration(parser_t *p)
{
    return parse_tagged_decl(p, TOK_KW_UNION,
                             parser_parse_union_decl,
                             parser_parse_union_var_decl);
}

/*
 * Attempt to parse a simple variable declaration beginning at the current
 * position.  The parser state is restored if no declaration is present.
 */
static stmt_t *maybe_parse_var_decl(parser_t *p)
{
    token_t *tok = peek(p);
    if (!tok)
        return NULL;

    if (tok->type == TOK_KW_STATIC || tok->type == TOK_KW_REGISTER ||
        tok->type == TOK_KW_CONST || tok->type == TOK_KW_VOLATILE ||
        tok->type == TOK_KW_INT || tok->type == TOK_KW_CHAR ||
        tok->type == TOK_KW_FLOAT || tok->type == TOK_KW_DOUBLE ||
        tok->type == TOK_KW_SHORT || tok->type == TOK_KW_LONG ||
        tok->type == TOK_KW_BOOL || tok->type == TOK_KW_UNSIGNED)
        return parser_parse_var_decl(p);

    return NULL;
}

/* Parse a variable or type declaration statement if present. */
/*
 * Attempt to parse any kind of declaration at statement scope.
 * Each helper leaves the parser position unchanged on failure so
 * we can simply try them in sequence.
 */
static stmt_t *parse_declaration_stmt(parser_t *p)
{
    stmt_t *s = parser_parse_static_assert(p);
    if (!s)
        s = parse_enum_declaration(p);
    if (!s)
        s = parse_struct_declaration(p);
    if (!s)
        s = parse_union_declaration(p);
    if (!s)
        s = maybe_parse_var_decl(p);
    return s;
}

/* Parse return, break, continue and goto statements. */
static stmt_t *parse_jump_stmt(parser_t *p)
{
    if (match(p, TOK_KW_RETURN)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        expr_t *expr = NULL;
        if (!match(p, TOK_SEMI)) {
            expr = parser_parse_expr(p);
            if (!expr || !match(p, TOK_SEMI)) {
                ast_free_expr(expr);
                return NULL;
            }
        }
        return ast_make_return(expr, kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_BREAK)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        if (!match(p, TOK_SEMI))
            return NULL;
        return ast_make_break(kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_CONTINUE)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        if (!match(p, TOK_SEMI))
            return NULL;
        return ast_make_continue(kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_GOTO)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        token_t *id = peek(p);
        if (!id || id->type != TOK_IDENT)
            return NULL;
        p->pos++;
        char *name = id->lexeme;
        if (!match(p, TOK_SEMI))
            return NULL;
        return ast_make_goto(name, kw_tok->line, kw_tok->column);
    }

    return NULL;
}

/* Dispatch to control flow helpers for if/while/do/for/switch. */
static stmt_t *parse_flow_stmt(parser_t *p)
{
    token_t *tok = peek(p);
    if (!tok)
        return NULL;

    if (tok->type == TOK_KW_IF)
        return parser_parse_if_stmt(p);
    if (tok->type == TOK_KW_WHILE)
        return parser_parse_while_stmt(p);
    if (tok->type == TOK_KW_DO)
        return parser_parse_do_while_stmt(p);
    if (tok->type == TOK_KW_FOR)
        return parser_parse_for_stmt(p);
    if (tok->type == TOK_KW_SWITCH)
        return parser_parse_switch_stmt(p);

    return NULL;
}




/*
 * Parse a single statement at the current position.  This function
 * delegates to the specific helpers for declarations, control flow
 * constructs and expression statements.  It returns the constructed
 * stmt_t or NULL on failure.
 */
stmt_t *parser_parse_stmt(parser_t *p)
{
    token_t *tok = peek(p);
    if (tok && tok->type == TOK_LBRACE)
        return parse_block(p);

    if (match(p, TOK_LABEL)) {
        token_t *lbl = &p->tokens[p->pos - 1];
        return ast_make_label(lbl->lexeme, lbl->line, lbl->column);
    }

    stmt_t *s = parse_declaration_stmt(p);
    if (s)
        return s;

    s = parse_jump_stmt(p);
    if (s)
        return s;

    s = parse_flow_stmt(p);
    if (s)
        return s;

    expr_t *expr = parser_parse_expr(p);
    if (!expr || !match(p, TOK_SEMI)) {
        ast_free_expr(expr);
        return NULL;
    }
    return ast_make_expr_stmt(expr, expr->line, expr->column);
}
