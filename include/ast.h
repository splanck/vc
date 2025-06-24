#ifndef VC_AST_H
#define VC_AST_H

#include <stddef.h>

/* Basic type categories used for type checking and function signatures */
typedef enum {
    TYPE_INT,
    TYPE_PTR,
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
    EXPR_CALL
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
    UNOP_DEREF
} unop_t;

struct expr;
struct stmt;
struct func;

typedef struct expr expr_t;
typedef struct stmt stmt_t;
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
            char *name;
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
    STMT_FOR
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
            /* optional initializer expression */
            expr_t *init;
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
            expr_t *init;
            expr_t *cond;
            expr_t *incr;
            stmt_t *body;
        } for_stmt;
    };
};

/* Constructors */
expr_t *ast_make_number(const char *value, size_t line, size_t column);
expr_t *ast_make_ident(const char *name, size_t line, size_t column);
expr_t *ast_make_string(const char *value, size_t line, size_t column);
expr_t *ast_make_char(char value, size_t line, size_t column);
expr_t *ast_make_binary(binop_t op, expr_t *left, expr_t *right,
                        size_t line, size_t column);
expr_t *ast_make_unary(unop_t op, expr_t *operand,
                       size_t line, size_t column);
expr_t *ast_make_assign(const char *name, expr_t *value,
                        size_t line, size_t column);
expr_t *ast_make_call(const char *name, size_t line, size_t column);

stmt_t *ast_make_expr_stmt(expr_t *expr, size_t line, size_t column);
stmt_t *ast_make_return(expr_t *expr, size_t line, size_t column);
stmt_t *ast_make_var_decl(const char *name, type_kind_t type, expr_t *init,
                          size_t line, size_t column);
stmt_t *ast_make_if(expr_t *cond, stmt_t *then_branch, stmt_t *else_branch,
                    size_t line, size_t column);
stmt_t *ast_make_while(expr_t *cond, stmt_t *body,
                       size_t line, size_t column);
stmt_t *ast_make_for(expr_t *init, expr_t *cond, expr_t *incr, stmt_t *body,
                     size_t line, size_t column);

/* Destructors */
void ast_free_expr(expr_t *expr);
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

func_t *ast_make_func(const char *name, type_kind_t ret_type,
                      char **param_names, type_kind_t *param_types,
                      size_t param_count,
                      stmt_t **body, size_t body_count);
void ast_free_func(func_t *func);

#endif /* VC_AST_H */
