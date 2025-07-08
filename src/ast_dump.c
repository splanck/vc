#include <stdlib.h>
#include "ast_dump.h"
#include "strbuf.h"

static void indent(strbuf_t *sb, int level)
{
    for (int i = 0; i < level; i++)
        strbuf_append(sb, "  ");
}

static const char *expr_name(expr_kind_t k)
{
    switch (k) {
    case EXPR_NUMBER: return "EXPR_NUMBER";
    case EXPR_IDENT: return "EXPR_IDENT";
    case EXPR_STRING: return "EXPR_STRING";
    case EXPR_CHAR: return "EXPR_CHAR";
    case EXPR_UNARY: return "EXPR_UNARY";
    case EXPR_BINARY: return "EXPR_BINARY";
    case EXPR_COND: return "EXPR_COND";
    case EXPR_ASSIGN: return "EXPR_ASSIGN";
    case EXPR_CALL: return "EXPR_CALL";
    case EXPR_INDEX: return "EXPR_INDEX";
    case EXPR_ASSIGN_INDEX: return "EXPR_ASSIGN_INDEX";
    case EXPR_ASSIGN_MEMBER: return "EXPR_ASSIGN_MEMBER";
    case EXPR_MEMBER: return "EXPR_MEMBER";
    case EXPR_SIZEOF: return "EXPR_SIZEOF";
    case EXPR_CAST: return "EXPR_CAST";
    case EXPR_COMPLIT: return "EXPR_COMPLIT";
    }
    return "EXPR_UNKNOWN";
}

static const char *stmt_name(stmt_kind_t k)
{
    switch (k) {
    case STMT_EXPR: return "STMT_EXPR";
    case STMT_RETURN: return "STMT_RETURN";
    case STMT_VAR_DECL: return "STMT_VAR_DECL";
    case STMT_IF: return "STMT_IF";
    case STMT_WHILE: return "STMT_WHILE";
    case STMT_DO_WHILE: return "STMT_DO_WHILE";
    case STMT_FOR: return "STMT_FOR";
    case STMT_SWITCH: return "STMT_SWITCH";
    case STMT_BREAK: return "STMT_BREAK";
    case STMT_CONTINUE: return "STMT_CONTINUE";
    case STMT_LABEL: return "STMT_LABEL";
    case STMT_GOTO: return "STMT_GOTO";
    case STMT_STATIC_ASSERT: return "STMT_STATIC_ASSERT";
    case STMT_TYPEDEF: return "STMT_TYPEDEF";
    case STMT_ENUM_DECL: return "STMT_ENUM_DECL";
    case STMT_STRUCT_DECL: return "STMT_STRUCT_DECL";
    case STMT_UNION_DECL: return "STMT_UNION_DECL";
    case STMT_BLOCK: return "STMT_BLOCK";
    }
    return "STMT_UNKNOWN";
}

static void dump_expr(strbuf_t *sb, const expr_t *e, int lvl);
static void dump_stmt(strbuf_t *sb, const stmt_t *s, int lvl);

static void dump_expr(strbuf_t *sb, const expr_t *e, int lvl)
{
    if (!e)
        return;
    indent(sb, lvl);
    strbuf_appendf(sb, "%s", expr_name(e->kind));
    if (e->kind == EXPR_NUMBER)
        strbuf_appendf(sb, " %s", e->number.value);
    else if (e->kind == EXPR_IDENT)
        strbuf_appendf(sb, " %s", e->ident.name);
    else if (e->kind == EXPR_STRING)
        strbuf_appendf(sb, " \"%s\"", e->string.value);
    else if (e->kind == EXPR_CHAR)
        strbuf_appendf(sb, " '%c'", e->ch.value);
    strbuf_append(sb, "\n");

    switch (e->kind) {
    case EXPR_UNARY:
        dump_expr(sb, e->unary.operand, lvl + 1);
        break;
    case EXPR_BINARY:
        dump_expr(sb, e->binary.left, lvl + 1);
        dump_expr(sb, e->binary.right, lvl + 1);
        break;
    case EXPR_COND:
        dump_expr(sb, e->cond.cond, lvl + 1);
        dump_expr(sb, e->cond.then_expr, lvl + 1);
        dump_expr(sb, e->cond.else_expr, lvl + 1);
        break;
    case EXPR_ASSIGN:
        dump_expr(sb, e->assign.value, lvl + 1);
        break;
    case EXPR_CALL:
        for (size_t i = 0; i < e->call.arg_count; i++)
            dump_expr(sb, e->call.args[i], lvl + 1);
        break;
    case EXPR_INDEX:
        dump_expr(sb, e->index.array, lvl + 1);
        dump_expr(sb, e->index.index, lvl + 1);
        break;
    case EXPR_ASSIGN_INDEX:
        dump_expr(sb, e->assign_index.array, lvl + 1);
        dump_expr(sb, e->assign_index.index, lvl + 1);
        dump_expr(sb, e->assign_index.value, lvl + 1);
        break;
    case EXPR_ASSIGN_MEMBER:
        dump_expr(sb, e->assign_member.object, lvl + 1);
        dump_expr(sb, e->assign_member.value, lvl + 1);
        break;
    case EXPR_MEMBER:
        dump_expr(sb, e->member.object, lvl + 1);
        break;
    case EXPR_SIZEOF:
        if (!e->sizeof_expr.is_type)
            dump_expr(sb, e->sizeof_expr.expr, lvl + 1);
        break;
    case EXPR_CAST:
        dump_expr(sb, e->cast.expr, lvl + 1);
        break;
    case EXPR_COMPLIT:
        if (e->compound.init)
            dump_expr(sb, e->compound.init, lvl + 1);
        for (size_t i = 0; i < e->compound.init_count; i++)
            dump_expr(sb, e->compound.init_list[i].value, lvl + 1);
        break;
    default:
        break;
    }
}

static void dump_block(strbuf_t *sb, stmt_t **stmts, size_t count, int lvl)
{
    for (size_t i = 0; i < count; i++)
        dump_stmt(sb, stmts[i], lvl);
}

static void dump_stmt(strbuf_t *sb, const stmt_t *s, int lvl)
{
    if (!s)
        return;
    indent(sb, lvl);
    strbuf_appendf(sb, "%s\n", stmt_name(s->kind));
    switch (s->kind) {
    case STMT_EXPR:
        dump_expr(sb, s->expr.expr, lvl + 1);
        break;
    case STMT_RETURN:
        dump_expr(sb, s->ret.expr, lvl + 1);
        break;
    case STMT_VAR_DECL:
        if (s->var_decl.init)
            dump_expr(sb, s->var_decl.init, lvl + 1);
        break;
    case STMT_IF:
        dump_expr(sb, s->if_stmt.cond, lvl + 1);
        dump_stmt(sb, s->if_stmt.then_branch, lvl + 1);
        dump_stmt(sb, s->if_stmt.else_branch, lvl + 1);
        break;
    case STMT_WHILE:
        dump_expr(sb, s->while_stmt.cond, lvl + 1);
        dump_stmt(sb, s->while_stmt.body, lvl + 1);
        break;
    case STMT_DO_WHILE:
        dump_stmt(sb, s->do_while_stmt.body, lvl + 1);
        dump_expr(sb, s->do_while_stmt.cond, lvl + 1);
        break;
    case STMT_FOR:
        if (s->for_stmt.init_decl)
            dump_stmt(sb, s->for_stmt.init_decl, lvl + 1);
        else
            dump_expr(sb, s->for_stmt.init, lvl + 1);
        dump_expr(sb, s->for_stmt.cond, lvl + 1);
        dump_expr(sb, s->for_stmt.incr, lvl + 1);
        dump_stmt(sb, s->for_stmt.body, lvl + 1);
        break;
    case STMT_SWITCH:
        dump_expr(sb, s->switch_stmt.expr, lvl + 1);
        for (size_t i = 0; i < s->switch_stmt.case_count; i++) {
            dump_expr(sb, s->switch_stmt.cases[i].expr, lvl + 1);
            dump_stmt(sb, s->switch_stmt.cases[i].body, lvl + 2);
        }
        dump_stmt(sb, s->switch_stmt.default_body, lvl + 1);
        break;
    case STMT_LABEL:
        indent(sb, lvl + 1);
        if (s->label.name)
            strbuf_appendf(sb, "label: %s\n", s->label.name);
        break;
    case STMT_GOTO:
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_STATIC_ASSERT:
    case STMT_TYPEDEF:
    case STMT_ENUM_DECL:
    case STMT_STRUCT_DECL:
    case STMT_UNION_DECL:
        break;
    case STMT_BLOCK:
        dump_block(sb, s->block.stmts, s->block.count, lvl + 1);
        break;
    }
}

static void dump_func(strbuf_t *sb, const func_t *f)
{
    strbuf_appendf(sb, "FUNC %s\n", f->name);
    dump_block(sb, f->body, f->body_count, 1);
}

char *ast_to_string(func_t **funcs, size_t fcount,
                    stmt_t **globs, size_t gcount)
{
    strbuf_t sb;
    strbuf_init(&sb);
    for (size_t i = 0; i < gcount; i++)
        dump_stmt(&sb, globs[i], 0);
    for (size_t i = 0; i < fcount; i++)
        dump_func(&sb, funcs[i]);
    return sb.data; /* caller frees */
}
