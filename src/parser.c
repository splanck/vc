#include <stdlib.h>
#include <stdio.h>
#include "parser.h"

static token_t *peek(parser_t *p)
{
    if (p->pos >= p->count)
        return NULL;
    return &p->tokens[p->pos];
}

static int match(parser_t *p, token_type_t type)
{
    token_t *tok = peek(p);
    if (tok && tok->type == type) {
        p->pos++;
        return 1;
    }
    return 0;
}

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
    if (tok) {
        fprintf(stderr, "Unexpected token '%s' at line %zu, column %zu",
                tok->lexeme, tok->line, tok->column);
    } else {
        fprintf(stderr, "Unexpected end of file");
    }

    if (expected_count > 0) {
        fprintf(stderr, ", expected ");
        for (size_t i = 0; i < expected_count; i++) {
            fprintf(stderr, "%s", token_name(expected[i]));
            if (i + 1 < expected_count)
                fprintf(stderr, ", ");
        }
    }

    fprintf(stderr, "\n");
}

/* Forward declarations */
static expr_t *parse_expression(parser_t *p);
static expr_t **parse_init_list(parser_t *p, size_t *out_count);
static expr_t *parse_assignment(parser_t *p);
static expr_t *parse_equality(parser_t *p);
static expr_t *parse_relational(parser_t *p);
static expr_t *parse_additive(parser_t *p);
static expr_t *parse_primary(parser_t *p)
{
    token_t *tok = peek(p);
    if (!tok)
        return NULL;

    if (match(p, TOK_STAR)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *op = parse_primary(p);
        if (!op)
            return NULL;
        return ast_make_unary(UNOP_DEREF, op, op_tok->line, op_tok->column);
    } else if (match(p, TOK_AMP)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *op = parse_primary(p);
        if (!op)
            return NULL;
        return ast_make_unary(UNOP_ADDR, op, op_tok->line, op_tok->column);
    } else if (match(p, TOK_MINUS)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        expr_t *op = parse_primary(p);
        if (!op)
            return NULL;
        return ast_make_unary(UNOP_NEG, op, op_tok->line, op_tok->column);
    }

    expr_t *base = NULL;
    if (match(p, TOK_NUMBER)) {
        base = ast_make_number(tok->lexeme, tok->line, tok->column);
    } else if (match(p, TOK_STRING)) {
        base = ast_make_string(tok->lexeme, tok->line, tok->column);
    } else if (match(p, TOK_CHAR)) {
        base = ast_make_char(tok->lexeme[0], tok->line, tok->column);
    } else if (tok->type == TOK_IDENT) {
        token_t *next = p->pos + 1 < p->count ? &p->tokens[p->pos + 1] : NULL;
        if (next && next->type == TOK_LPAREN) {
            p->pos++; /* consume ident */
            char *name = tok->lexeme;
            match(p, TOK_LPAREN);
            size_t cap = 4, count = 0;
            expr_t **args = malloc(cap * sizeof(*args));
            if (!args)
                return NULL;
            if (!match(p, TOK_RPAREN)) {
                do {
                    expr_t *arg = parse_expression(p);
                    if (!arg) {
                        for (size_t i = 0; i < count; i++)
                            ast_free_expr(args[i]);
                        free(args);
                        return NULL;
                    }
                    if (count >= cap) {
                        cap *= 2;
                        expr_t **tmp = realloc(args, cap * sizeof(*tmp));
                        if (!tmp) {
                            for (size_t i = 0; i < count; i++)
                                ast_free_expr(args[i]);
                            free(args);
                            return NULL;
                        }
                        args = tmp;
                    }
                    args[count++] = arg;
                } while (match(p, TOK_COMMA));
                if (!match(p, TOK_RPAREN)) {
                    for (size_t i = 0; i < count; i++)
                        ast_free_expr(args[i]);
                    free(args);
                    return NULL;
                }
            }
            expr_t *call = ast_make_call(name, args, count,
                                         tok->line, tok->column);
            base = call;
        } else if (match(p, TOK_IDENT)) {
            base = ast_make_ident(tok->lexeme, tok->line, tok->column);
        }
    } else if (match(p, TOK_LPAREN)) {
        expr_t *expr = parse_expression(p);
        if (!match(p, TOK_RPAREN)) {
            ast_free_expr(expr);
            return NULL;
        }
        base = expr;
    }
    if (!base)
        return NULL;
    while (match(p, TOK_LBRACKET)) {
        token_t *lb = &p->tokens[p->pos - 1];
        expr_t *idx = parse_expression(p);
        if (!idx || !match(p, TOK_RBRACKET)) {
            ast_free_expr(base);
            ast_free_expr(idx);
            return NULL;
        }
        base = ast_make_index(base, idx, lb->line, lb->column);
    }
    return base;
}

static expr_t *parse_term(parser_t *p)
{
    expr_t *left = parse_primary(p);
    if (!left)
        return NULL;

    while (1) {
        if (match(p, TOK_STAR)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_primary(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_MUL, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_SLASH)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_primary(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_DIV, left, right,
                                  op_tok->line, op_tok->column);
        } else {
            break;
        }
    }
    return left;
}

static expr_t *parse_additive(parser_t *p)
{
    expr_t *left = parse_term(p);
    if (!left)
        return NULL;

    while (1) {
        if (match(p, TOK_PLUS)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_term(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_ADD, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_MINUS)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_term(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_SUB, left, right,
                                  op_tok->line, op_tok->column);
        } else {
            break;
        }
    }
    return left;
}

static expr_t *parse_relational(parser_t *p)
{
    expr_t *left = parse_additive(p);
    if (!left)
        return NULL;

    while (1) {
        if (match(p, TOK_LT)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_additive(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_LT, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_GT)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_additive(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_GT, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_LE)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_additive(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_LE, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_GE)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_additive(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_GE, left, right,
                                  op_tok->line, op_tok->column);
        } else {
            break;
        }
    }
    return left;
}

static expr_t *parse_equality(parser_t *p)
{
    expr_t *left = parse_relational(p);
    if (!left)
        return NULL;

    while (1) {
        if (match(p, TOK_EQ)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_relational(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_EQ, left, right,
                                  op_tok->line, op_tok->column);
        } else if (match(p, TOK_NEQ)) {
            token_t *op_tok = &p->tokens[p->pos - 1];
            expr_t *right = parse_relational(p);
            if (!right) {
                ast_free_expr(left);
                return NULL;
            }
            left = ast_make_binary(BINOP_NEQ, left, right,
                                  op_tok->line, op_tok->column);
        } else {
            break;
        }
    }
    return left;
}

static expr_t *parse_assignment(parser_t *p)
{
    expr_t *left = parse_equality(p);
    if (!left)
        return NULL;

    if (match(p, TOK_ASSIGN)) {
        token_t *op_tok = &p->tokens[p->pos - 1];
        if (left->kind != EXPR_IDENT && left->kind != EXPR_INDEX) {
            ast_free_expr(left);
            return NULL;
        }
        expr_t *right = parse_assignment(p);
        if (!right) {
            if (left->kind == EXPR_IDENT)
                free(left->ident.name);
            ast_free_expr(left);
            return NULL;
        }
        if (left->kind == EXPR_IDENT) {
            char *name = left->ident.name;
            free(left);
            left = ast_make_assign(name, right, op_tok->line, op_tok->column);
            free(name);
        } else {
            expr_t *arr = left->index.array;
            expr_t *idx = left->index.index;
            free(left);
            left = ast_make_assign_index(arr, idx, right,
                                        op_tok->line, op_tok->column);
        }
    }
    return left;
}

static expr_t *parse_expression(parser_t *p)
{
    return parse_assignment(p);
}

static expr_t **parse_init_list(parser_t *p, size_t *out_count)
{
    if (!match(p, TOK_LBRACE))
        return NULL;
    size_t cap = 4, count = 0;
    expr_t **vals = malloc(cap * sizeof(*vals));
    if (!vals)
        return NULL;
    if (!match(p, TOK_RBRACE)) {
        do {
            expr_t *e = parse_expression(p);
            if (!e) {
                for (size_t i = 0; i < count; i++)
                    ast_free_expr(vals[i]);
                free(vals);
                return NULL;
            }
            if (count >= cap) {
                cap *= 2;
                expr_t **tmp = realloc(vals, cap * sizeof(*tmp));
                if (!tmp) {
                    for (size_t i = 0; i < count; i++)
                        ast_free_expr(vals[i]);
                    free(vals);
                    return NULL;
                }
                vals = tmp;
            }
            vals[count++] = e;
        } while (match(p, TOK_COMMA));
        if (!match(p, TOK_RBRACE)) {
            for (size_t i = 0; i < count; i++)
                ast_free_expr(vals[i]);
            free(vals);
            return NULL;
        }
    }
    if (out_count)
        *out_count = count;
    return vals;
}

static stmt_t *parse_block(parser_t *p)
{
    if (!match(p, TOK_LBRACE))
        return NULL;
    token_t *lb_tok = &p->tokens[p->pos - 1];
    size_t cap = 4, count = 0;
    stmt_t **stmts = malloc(cap * sizeof(*stmts));
    if (!stmts)
        return NULL;
    while (!match(p, TOK_RBRACE)) {
        stmt_t *s = parser_parse_stmt(p);
        if (!s) {
            for (size_t i = 0; i < count; i++)
                ast_free_stmt(stmts[i]);
            free(stmts);
            return NULL;
        }
        if (count >= cap) {
            cap *= 2;
            stmt_t **tmp = realloc(stmts, cap * sizeof(*tmp));
            if (!tmp) {
                for (size_t i = 0; i < count; i++)
                    ast_free_stmt(stmts[i]);
                free(stmts);
                return NULL;
            }
            stmts = tmp;
        }
        stmts[count++] = s;
    }
    return ast_make_block(stmts, count, lb_tok->line, lb_tok->column);
}

expr_t *parser_parse_expr(parser_t *p)
{
    return parse_expression(p);
}

stmt_t *parser_parse_stmt(parser_t *p)
{
    token_t *tok = peek(p);
    if (tok && tok->type == TOK_LBRACE)
        return parse_block(p);

    if (match(p, TOK_KW_INT) || match(p, TOK_KW_CHAR)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        type_kind_t t = (kw_tok->type == TOK_KW_INT) ? TYPE_INT : TYPE_CHAR;
        if (match(p, TOK_STAR))
            t = TYPE_PTR;
        token_t *tok = peek(p);
        if (!tok || tok->type != TOK_IDENT)
            return NULL;
        p->pos++;
        char *name = tok->lexeme;
        size_t arr_size = 0;
        if (match(p, TOK_LBRACKET)) {
            token_t *num = peek(p);
            if (!num || num->type != TOK_NUMBER)
                return NULL;
            p->pos++;
            arr_size = strtoul(num->lexeme, NULL, 10);
            if (!match(p, TOK_RBRACKET))
                return NULL;
            t = TYPE_ARRAY;
        }
        expr_t *init = NULL;
        expr_t **init_list = NULL;
        size_t init_count = 0;
        if (match(p, TOK_ASSIGN)) {
            if (t == TYPE_ARRAY && peek(p) && peek(p)->type == TOK_LBRACE) {
                init_list = parse_init_list(p, &init_count);
                if (!init_list || !match(p, TOK_SEMI)) {
                    if (init_list) {
                        for (size_t i = 0; i < init_count; i++)
                            ast_free_expr(init_list[i]);
                        free(init_list);
                    }
                    return NULL;
                }
            } else {
                init = parse_expression(p);
                if (!init || !match(p, TOK_SEMI)) {
                    ast_free_expr(init);
                    return NULL;
                }
            }
        } else {
            if (!match(p, TOK_SEMI))
                return NULL;
        }
        return ast_make_var_decl(name, t, arr_size, init,
                                 init_list, init_count,
                                 kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_RETURN)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        expr_t *expr = NULL;
        if (!match(p, TOK_SEMI)) {
            expr = parse_expression(p);
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

    if (match(p, TOK_KW_IF)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        if (!match(p, TOK_LPAREN))
            return NULL;
        expr_t *cond = parse_expression(p);
        if (!cond || !match(p, TOK_RPAREN)) {
            ast_free_expr(cond);
            return NULL;
        }
        stmt_t *then_branch = parser_parse_stmt(p);
        if (!then_branch) {
            ast_free_expr(cond);
            return NULL;
        }
        stmt_t *else_branch = NULL;
        if (match(p, TOK_KW_ELSE)) {
            else_branch = parser_parse_stmt(p);
            if (!else_branch) {
                ast_free_expr(cond);
                ast_free_stmt(then_branch);
                return NULL;
            }
        }
        return ast_make_if(cond, then_branch, else_branch,
                           kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_WHILE)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        if (!match(p, TOK_LPAREN))
            return NULL;
        expr_t *cond = parse_expression(p);
        if (!cond || !match(p, TOK_RPAREN)) {
            ast_free_expr(cond);
            return NULL;
        }
        stmt_t *body = parser_parse_stmt(p);
        if (!body) {
            ast_free_expr(cond);
            return NULL;
        }
        return ast_make_while(cond, body, kw_tok->line, kw_tok->column);
    }

    if (match(p, TOK_KW_FOR)) {
        token_t *kw_tok = &p->tokens[p->pos - 1];
        if (!match(p, TOK_LPAREN))
            return NULL;
        expr_t *init = parse_expression(p);
        if (!init || !match(p, TOK_SEMI)) {
            ast_free_expr(init);
            return NULL;
        }
        expr_t *cond = parse_expression(p);
        if (!cond || !match(p, TOK_SEMI)) {
            ast_free_expr(init);
            ast_free_expr(cond);
            return NULL;
        }
        expr_t *incr = parse_expression(p);
        if (!incr || !match(p, TOK_RPAREN)) {
            ast_free_expr(init);
            ast_free_expr(cond);
            ast_free_expr(incr);
            return NULL;
        }
        stmt_t *body = parser_parse_stmt(p);
        if (!body) {
            ast_free_expr(init);
            ast_free_expr(cond);
            ast_free_expr(incr);
            return NULL;
        }
        return ast_make_for(init, cond, incr, body,
                            kw_tok->line, kw_tok->column);
    }

    expr_t *expr = parse_expression(p);
    if (!expr || !match(p, TOK_SEMI)) {
        ast_free_expr(expr);
        return NULL;
    }
    return ast_make_expr_stmt(expr, expr->line, expr->column);
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

    size_t pcap = 4, pcount = 0;
    char **param_names = malloc(pcap * sizeof(*param_names));
    type_kind_t *param_types = malloc(pcap * sizeof(*param_types));
    if (!param_names || !param_types) {
        free(param_names);
        free(param_types);
        return NULL;
    }

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
                free(param_names);
                free(param_types);
                return NULL;
            }
            token_t *ptok = peek(p);
            if (!ptok || ptok->type != TOK_IDENT) {
                free(param_names);
                free(param_types);
                return NULL;
            }
            p->pos++;
            if (pcount >= pcap) {
                pcap *= 2;
                char **n1 = realloc(param_names, pcap * sizeof(*n1));
                type_kind_t *n2 = realloc(param_types, pcap * sizeof(*n2));
                if (!n1 || !n2) {
                    free(n1 ? n1 : param_names);
                    free(n2 ? n2 : param_types);
                    return NULL;
                }
                param_names = n1;
                param_types = n2;
            }
            param_names[pcount] = ptok->lexeme;
            param_types[pcount] = pt;
            pcount++;
        } while (match(p, TOK_COMMA));
        if (!match(p, TOK_RPAREN)) {
            free(param_names);
            free(param_types);
            return NULL;
        }
    }

    if (!match(p, TOK_LBRACE)) {
        free(param_names);
        free(param_types);
        return NULL;
    }

    size_t cap = 4, count = 0;
    stmt_t **body = malloc(cap * sizeof(*body));
    if (!body) {
        free(param_names);
        free(param_types);
        return NULL;
    }

    while (!match(p, TOK_RBRACE)) {
        stmt_t *stmt = parser_parse_stmt(p);
        if (!stmt) {
            for (size_t i = 0; i < count; i++)
                ast_free_stmt(body[i]);
            free(body);
            free(param_names);
            free(param_types);
            return NULL;
        }
        if (count >= cap) {
            cap *= 2;
            stmt_t **tmp = realloc(body, cap * sizeof(*tmp));
            if (!tmp) {
                for (size_t i = 0; i < count; i++)
                    ast_free_stmt(body[i]);
                free(body);
                free(param_names);
                free(param_types);
                return NULL;
            }
            body = tmp;
        }
        body[count++] = stmt;
    }

    func_t *fn = ast_make_func(name, ret_type,
                               param_names, param_types, pcount,
                               body, count);
    free(param_names);
    free(param_types);
    if (!fn) {
        for (size_t i = 0; i < count; i++)
            ast_free_stmt(body[i]);
        free(body);
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
            init_list = parse_init_list(p, &init_count);
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
