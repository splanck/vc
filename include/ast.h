/*
 * AST node definitions for the compiler.
 *
 * The abstract syntax tree is built from a small set of node
 * structures.  Every expression or statement node begins with a
 * ``kind'' tag along with source line and column information.  The
 * remainder of each node is stored in a union so that only the fields
 * relevant to a particular kind are allocated.  Helper functions in
 * ``ast_expr.h'' and ``ast_stmt.h'' allocate and initialise these
 * nodes while maintaining this tagged union layout.
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
    TYPE_UINT,
    TYPE_CHAR,
    TYPE_UCHAR,
    TYPE_SHORT,
    TYPE_USHORT,
    TYPE_LONG,
    TYPE_ULONG,
    TYPE_LLONG,
    TYPE_ULLONG,
    TYPE_BOOL,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_LDOUBLE,
    TYPE_PTR,
    TYPE_ARRAY,
    TYPE_VOID,
    TYPE_ENUM,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_UNKNOWN
} type_kind_t;

/* Expression AST node types including struct/union member operations */
typedef enum {
    EXPR_NUMBER,
    EXPR_IDENT,
    EXPR_STRING,
    EXPR_CHAR,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_COND,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_INDEX,
    EXPR_ASSIGN_INDEX,
    EXPR_ASSIGN_MEMBER,
    EXPR_MEMBER,
    EXPR_SIZEOF,
    EXPR_COMPLIT
} expr_kind_t;

/* Binary operator types */
typedef enum {
    BINOP_ADD,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_DIV,
    BINOP_MOD,
    BINOP_SHL,
    BINOP_SHR,
    BINOP_BITAND,
    BINOP_BITXOR,
    BINOP_BITOR,
    BINOP_EQ,
    BINOP_NEQ,
    BINOP_LOGAND,
    BINOP_LOGOR,
    BINOP_LT,
    BINOP_GT,
    BINOP_LE,
    BINOP_GE
} binop_t;

typedef enum {
    UNOP_ADDR,
    UNOP_DEREF,
    UNOP_NEG,
    UNOP_NOT,
    UNOP_PREINC,
    UNOP_PREDEC,
    UNOP_POSTINC,
    UNOP_POSTDEC
} unop_t;

struct expr;
struct stmt;
struct switch_case;
struct enumerator;
struct union_member;
struct struct_member;
struct func;

typedef struct expr expr_t;
typedef struct stmt stmt_t;
typedef struct switch_case switch_case_t;
typedef struct enumerator enumerator_t;
typedef struct union_member union_member_t;
typedef struct struct_member struct_member_t;
typedef struct func func_t;

typedef enum { INIT_SIMPLE, INIT_FIELD, INIT_INDEX } init_kind_t;
typedef struct init_entry {
    init_kind_t kind;
    char *field;      /* for .name */
    expr_t *index;    /* for [expr] */
    expr_t *value;
} init_entry_t;

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
            expr_t *cond;
            expr_t *then_expr;
            expr_t *else_expr;
        } cond;
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
            expr_t *object;
            char *member;
            expr_t *value;
            int via_ptr;
        } assign_member;
        struct {
            expr_t *object;
            char *member;
            int via_ptr;
        } member;
        struct {
            char *name;
            expr_t **args;
            size_t arg_count;
        } call;
        struct {
            int is_type;
            type_kind_t type;
            size_t array_size;
            size_t elem_size;
            expr_t *expr;
        } sizeof_expr;
        struct {
            type_kind_t type;
            size_t array_size;
            size_t elem_size;
            expr_t *init;
            init_entry_t *init_list;
            size_t init_count;
        } compound;
    };
};

/* Statement AST node types including struct/union declarations */
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
    STMT_TYPEDEF,
    STMT_ENUM_DECL,
    STMT_STRUCT_DECL,
    STMT_UNION_DECL,
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
            expr_t *size_expr;
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
};

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
};

struct struct_member {
    char *name;
    type_kind_t type;
    size_t elem_size;
    size_t offset;
};


/* Function definition structure */
struct func {
    char *name;
    type_kind_t return_type;
    char **param_names;
    type_kind_t *param_types;
    size_t *param_elem_sizes;
    int *param_is_restrict;
    size_t param_count;
    stmt_t **body;
    size_t body_count;
};


#endif /* VC_AST_H */
