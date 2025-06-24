#ifndef VC_AST_H
#define VC_AST_H

#include <stddef.h>

/* Expression AST node types */
typedef enum {
    EXPR_NUMBER,
    EXPR_IDENT,
    EXPR_BINARY
} expr_kind_t;

/* Binary operator types */
typedef enum {
    BINOP_ADD,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_DIV
} binop_t;

struct expr;
struct stmt;

typedef struct expr expr_t;
typedef struct stmt stmt_t;

struct expr {
    expr_kind_t kind;
    union {
        struct {
            char *value;
        } number;
        struct {
            char *name;
        } ident;
        struct {
            binop_t op;
            expr_t *left;
            expr_t *right;
        } binary;
    };
};

/* Statement AST node types */
typedef enum {
    STMT_EXPR,
    STMT_RETURN
} stmt_kind_t;

struct stmt {
    stmt_kind_t kind;
    union {
        struct {
            expr_t *expr;
        } expr;
        struct {
            expr_t *expr;
        } ret;
    };
};

/* Constructors */
expr_t *ast_make_number(const char *value);
expr_t *ast_make_ident(const char *name);
expr_t *ast_make_binary(binop_t op, expr_t *left, expr_t *right);

stmt_t *ast_make_expr_stmt(expr_t *expr);
stmt_t *ast_make_return(expr_t *expr);

/* Destructors */
void ast_free_expr(expr_t *expr);
void ast_free_stmt(stmt_t *stmt);

#endif /* VC_AST_H */
