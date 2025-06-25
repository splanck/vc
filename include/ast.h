/*
 * AST node definitions for the compiler.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_AST_H
#define VC_AST_H

#include <stddef.h>

/* Basic type categories used for type checking and function signatures */
typedef enum {
    TYPE_INT,
    TYPE_CHAR,
    TYPE_PTR,
    TYPE_ARRAY,
    TYPE_VOID,
    TYPE_UNKNOWN
} type_kind_t;

/* Expression AST node types */
typedef enum {
    EXPR_NUMBER,
    EXPR_IDENT,
    EXPR_STRING,
    EXPR_CHAR,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_INDEX,
    EXPR_ASSIGN_INDEX
} expr_kind_t;

/* Binary operator types */
typedef enum {
    BINOP_ADD,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_DIV,
    BINOP_EQ,
    BINOP_NEQ,
    BINOP_LT,
    BINOP_GT,
    BINOP_LE,
    BINOP_GE
} binop_t;

typedef enum {
    UNOP_ADDR,
    UNOP_DEREF,
    UNOP_NEG
} unop_t;

struct expr;
struct stmt;
struct switch_case;
struct func;

typedef struct expr expr_t;
typedef struct stmt stmt_t;
typedef struct switch_case switch_case_t;
typedef struct func func_t;

struct expr {
    expr_kind_t kind;
    size_t line;
    size_t column;
    union {
        struct {
            char *value;
        } number;
        struct {
            char *name;
        } ident;
        struct {
            char *value;
        } string;
        struct {
            char value;
        } ch;
        struct {
            unop_t op;
            expr_t *operand;
        } unary;
        struct {
            binop_t op;
            expr_t *left;
            expr_t *right;
        } binary;
        struct {
            char *name;
            expr_t *value;
        } assign;
        struct {
            expr_t *array;
            expr_t *index;
        } index;
        struct {
            expr_t *array;
            expr_t *index;
            expr_t *value;
        } assign_index;
        struct {
            char *name;
            expr_t **args;
            size_t arg_count;
        } call;
    };
};

/* Statement AST node types */
typedef enum {
    STMT_EXPR,
    STMT_RETURN,
    STMT_VAR_DECL,
    STMT_IF,
    STMT_WHILE,
    STMT_DO_WHILE,
    STMT_FOR,
    STMT_SWITCH,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_LABEL,
    STMT_GOTO,
    STMT_BLOCK
} stmt_kind_t;

struct stmt {
    stmt_kind_t kind;
    size_t line;
    size_t column;
    union {
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
            /* optional initializer expression */
            expr_t *init;
            /* optional initializer list for arrays */
            expr_t **init_list;
            size_t init_count;
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
            expr_t *init;
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
            stmt_t **stmts;
            size_t count;
        } block;
    };
};

struct switch_case {
    expr_t *expr;
    stmt_t *body;
};

/* Constructors */
/* Create a numeric literal expression from the given string representation. */
expr_t *ast_make_number(const char *value, size_t line, size_t column);
/* Create an identifier expression holding the provided name. */
expr_t *ast_make_ident(const char *name, size_t line, size_t column);
/* Create a string literal expression. */
expr_t *ast_make_string(const char *value, size_t line, size_t column);
/* Create a character literal expression. */
expr_t *ast_make_char(char value, size_t line, size_t column);
/* Create a binary operation node with the supplied operands. */
expr_t *ast_make_binary(binop_t op, expr_t *left, expr_t *right,
                        size_t line, size_t column);
/* Create a unary operation node. */
expr_t *ast_make_unary(unop_t op, expr_t *operand,
                       size_t line, size_t column);
/* Create an assignment expression to the variable \p name. */
expr_t *ast_make_assign(const char *name, expr_t *value,
                        size_t line, size_t column);
/* Create an array indexing expression. */
expr_t *ast_make_index(expr_t *array, expr_t *index,
                       size_t line, size_t column);
/* Create an array element assignment expression. */
expr_t *ast_make_assign_index(expr_t *array, expr_t *index, expr_t *value,
                              size_t line, size_t column);
/* Create a function call expression with \p arg_count arguments. */
expr_t *ast_make_call(const char *name, expr_t **args, size_t arg_count,
                      size_t line, size_t column);

/* Create an expression statement node. */
stmt_t *ast_make_expr_stmt(expr_t *expr, size_t line, size_t column);
/* Create a return statement. The expression may be NULL for 'return;'. */
stmt_t *ast_make_return(expr_t *expr, size_t line, size_t column);
/* Declare a variable optionally initialized by \p init or \p init_list. */
stmt_t *ast_make_var_decl(const char *name, type_kind_t type, size_t array_size,
                          expr_t *init, expr_t **init_list, size_t init_count,
                          size_t line, size_t column);
/* Create an if/else statement. \p else_branch may be NULL. */
stmt_t *ast_make_if(expr_t *cond, stmt_t *then_branch, stmt_t *else_branch,
                    size_t line, size_t column);
/* Construct a while loop statement. */
stmt_t *ast_make_while(expr_t *cond, stmt_t *body,
                       size_t line, size_t column);
/* Construct a do-while loop statement. */
stmt_t *ast_make_do_while(expr_t *cond, stmt_t *body,
                         size_t line, size_t column);
/* Construct a for loop statement with optional init/cond/incr expressions. */
stmt_t *ast_make_for(expr_t *init, expr_t *cond, expr_t *incr, stmt_t *body,
                     size_t line, size_t column);
/* Construct a switch statement with optional default block. */
stmt_t *ast_make_switch(expr_t *expr, switch_case_t *cases, size_t case_count,
                        stmt_t *default_body, size_t line, size_t column);
/* Simple break statement used inside loops. */
stmt_t *ast_make_break(size_t line, size_t column);
/* Simple continue statement used inside loops. */
stmt_t *ast_make_continue(size_t line, size_t column);
/* Label definition statement */
stmt_t *ast_make_label(const char *name, size_t line, size_t column);
/* goto statement */
stmt_t *ast_make_goto(const char *name, size_t line, size_t column);
/* Create a block of statements containing \p count elements. */
stmt_t *ast_make_block(stmt_t **stmts, size_t count,
                       size_t line, size_t column);

/* Destructors */
/* Recursively free an expression tree. */
void ast_free_expr(expr_t *expr);
/* Free a statement and any child expressions or statements it owns. */
void ast_free_stmt(stmt_t *stmt);

/* Function definition structure */
struct func {
    char *name;
    type_kind_t return_type;
    char **param_names;
    type_kind_t *param_types;
    size_t param_count;
    stmt_t **body;
    size_t body_count;
};

/* Create a function definition node with the provided signature and body. */
func_t *ast_make_func(const char *name, type_kind_t ret_type,
                      char **param_names, type_kind_t *param_types,
                      size_t param_count,
                      stmt_t **body, size_t body_count);
/* Free a function and all statements contained in its body. */
void ast_free_func(func_t *func);

#endif /* VC_AST_H */
