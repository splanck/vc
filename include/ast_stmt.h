/*
 * Statement and function AST construction helpers.
 *
 * These routines mirror those in ``ast_expr.h'' but operate on the various
 * statement node types and complete function definitions.  Each constructor
 * allocates a new node and returns it to the caller, who is then
 * responsible for eventually freeing the structure with ``ast_free_stmt'' or
 * ``ast_free_func''.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_AST_STMT_H
#define VC_AST_STMT_H

#include "ast.h"
#include "ast_expr.h"

struct switch_case {
    expr_t *expr;
    stmt_t *body;
};

struct enumerator {
    char *name;
    expr_t *value; /* may be NULL */
};

struct union_member {
    char *name;
    type_kind_t type;
    size_t elem_size;
    size_t offset;
    unsigned bit_width;
    unsigned bit_offset;
    int is_flexible;
};

struct struct_member {
    char *name;
    type_kind_t type;
    size_t elem_size;
    size_t offset;
    unsigned bit_width;
    unsigned bit_offset;
    int is_flexible;
};

union stmt_data {
    struct {
        expr_t *expr;
    } expr;
    struct {
        /* expression may be NULL for 'return;' in void functions */
        expr_t *expr;
    } ret;
    struct {
        char *name;
        type_kind_t type;
        size_t array_size;
        expr_t *size_expr;
        expr_t *align_expr;
        size_t alignment;
        size_t elem_size;
        char *tag; /* NULL for basic types */
        int is_static;
        int is_register;
        int is_extern;
        int is_const;
        int is_volatile;
        int is_restrict;
        /* optional initializer expression */
        expr_t *init;
        /* optional initializer list for arrays */
        init_entry_t *init_list;
        size_t init_count;
        union_member_t *members;
        size_t member_count;
        /* function pointer metadata */
        type_kind_t func_ret_type;
        type_kind_t *func_param_types;
        size_t func_param_count;
        int func_variadic;
        /* additional declarators in the same statement */
        struct stmt **next;
        size_t next_count;
    } var_decl;
    struct {
        expr_t *cond;
        stmt_t *then_branch;
        stmt_t *else_branch; /* may be NULL */
    } if_stmt;
    struct {
        expr_t *cond;
        stmt_t *body;
    } while_stmt;
    struct {
        expr_t *cond;
        stmt_t *body;
    } do_while_stmt;
    struct {
        stmt_t *init_decl; /* optional variable declaration */
        expr_t *init;       /* optional init expression */
        expr_t *cond;
        expr_t *incr;
        stmt_t *body;
    } for_stmt;
    struct {
        expr_t *expr;
        switch_case_t *cases;
        size_t case_count;
        stmt_t *default_body; /* may be NULL */
    } switch_stmt;
    struct {
        char *name;
    } label;
    struct {
        char *name;
    } goto_stmt;
    struct {
        expr_t *expr;
        char *message;
    } static_assert;
    struct {
        char *name;
        type_kind_t type;
        size_t array_size;
        size_t elem_size;
    } typedef_decl;
    struct {
        char *tag;
        enumerator_t *items;
        size_t count;
    } enum_decl;
    struct {
        char *tag;
        struct_member_t *members;
        size_t count;
    } struct_decl;
    struct {
        char *tag;
        union_member_t *members;
        size_t count;
    } union_decl;
    struct {
        stmt_t **stmts;
        size_t count;
    } block;
};

struct stmt {
    stmt_kind_t kind;
    size_t line;
    size_t column;
    union stmt_data data;
};

/* Convenience macros for accessing statement data */
#define STMT_EXPR(s)        ((s)->data.expr)
#define STMT_RET(s)         ((s)->data.ret)
#define STMT_VAR_DECL(s)    ((s)->data.var_decl)
#define STMT_IF(s)          ((s)->data.if_stmt)
#define STMT_WHILE(s)       ((s)->data.while_stmt)
#define STMT_DO_WHILE(s)    ((s)->data.do_while_stmt)
#define STMT_FOR(s)         ((s)->data.for_stmt)
#define STMT_SWITCH(s)      ((s)->data.switch_stmt)
#define STMT_LABEL(s)       ((s)->data.label)
#define STMT_GOTO(s)        ((s)->data.goto_stmt)
#define STMT_STATIC_ASSERT(s) ((s)->data.static_assert)
#define STMT_TYPEDEF(s)     ((s)->data.typedef_decl)
#define STMT_ENUM_DECL(s)   ((s)->data.enum_decl)
#define STMT_STRUCT_DECL(s) ((s)->data.struct_decl)
#define STMT_UNION_DECL(s)  ((s)->data.union_decl)
#define STMT_BLOCK(s)       ((s)->data.block)

/* Function definition structure */
struct func {
    char *name;
    type_kind_t return_type;
    char *return_tag;
    char **param_names;
    type_kind_t *param_types;
    char **param_tags;
    size_t *param_elem_sizes;
    int *param_is_restrict;
    size_t param_count;
    int is_variadic;
    stmt_t **body;
    size_t body_count;
    int is_inline;
    int is_noreturn;
};

/* Create a statement from a single expression. */
stmt_t *ast_make_expr_stmt(expr_t *expr, size_t line, size_t column);
/* Create a return statement. */
stmt_t *ast_make_return(expr_t *expr, size_t line, size_t column);
/* Create a variable declaration statement. */
stmt_t *ast_make_var_decl(const char *name, type_kind_t type, size_t array_size,
                          expr_t *size_expr, expr_t *align_expr,
                          size_t elem_size, int is_static,
                          int is_register, int is_extern, int is_const,
                          int is_volatile, int is_restrict,
                          expr_t *init, init_entry_t *init_list, size_t init_count,
                          const char *tag, union_member_t *members,
                          size_t member_count, size_t line, size_t column);
/* Internal helper for allocating name/tag strings. */
int init_var_decl(stmt_t *stmt, const char *name, const char *tag);
/* Create an if statement. */
stmt_t *ast_make_if(expr_t *cond, stmt_t *then_branch, stmt_t *else_branch,
                    size_t line, size_t column);
/* Create a while loop. */
stmt_t *ast_make_while(expr_t *cond, stmt_t *body,
                       size_t line, size_t column);
/* Create a do-while loop. */
stmt_t *ast_make_do_while(expr_t *cond, stmt_t *body,
                          size_t line, size_t column);
/* Create a for loop. */
stmt_t *ast_make_for(stmt_t *init_decl, expr_t *init, expr_t *cond,
                     expr_t *incr, stmt_t *body,
                     size_t line, size_t column);
/* Create a switch statement. */
stmt_t *ast_make_switch(expr_t *expr, switch_case_t *cases, size_t case_count,
                        stmt_t *default_body, size_t line, size_t column);
/* Create a break statement. */
stmt_t *ast_make_break(size_t line, size_t column);
/* Create a continue statement. */
stmt_t *ast_make_continue(size_t line, size_t column);
/* Create a label statement. */
stmt_t *ast_make_label(const char *name, size_t line, size_t column);
/* Create a goto statement. */
stmt_t *ast_make_goto(const char *name, size_t line, size_t column);
/* Create a _Static_assert statement. */
stmt_t *ast_make_static_assert(expr_t *expr, const char *msg,
                               size_t line, size_t column);
/* Create a typedef declaration. */
stmt_t *ast_make_typedef(const char *name, type_kind_t type, size_t array_size,
                         size_t elem_size, size_t line, size_t column);
/* Create an enum declaration. */
stmt_t *ast_make_enum_decl(const char *tag, enumerator_t *items, size_t count,
                           size_t line, size_t column);
/* Create a struct declaration. */
stmt_t *ast_make_struct_decl(const char *tag, struct_member_t *members,
                             size_t count, size_t line, size_t column);
/* Create a union declaration. */
stmt_t *ast_make_union_decl(const char *tag, union_member_t *members,
                            size_t count, size_t line, size_t column);
/* Create a block containing an array of statements. */
stmt_t *ast_make_block(stmt_t **stmts, size_t count,
                       size_t line, size_t column);

/* Create a function definition. */
func_t *ast_make_func(const char *name, type_kind_t ret_type,
                      const char *ret_tag,
                      char **param_names, type_kind_t *param_types,
                      char **param_tags,
                      size_t *param_elem_sizes, int *param_is_restrict,
                      size_t param_count, int is_variadic,
                      stmt_t **body, size_t body_count,
                      int is_inline, int is_noreturn);

/* Recursively free a statement tree. */
void ast_free_stmt(stmt_t *stmt);
/* Free a function definition. */
void ast_free_func(func_t *func);

#endif /* VC_AST_STMT_H */
