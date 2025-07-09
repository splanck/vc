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
    TYPE_FLOAT_COMPLEX,
    TYPE_DOUBLE_COMPLEX,
    TYPE_LDOUBLE_COMPLEX,
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
    EXPR_COMPLEX_LITERAL,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_COND,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_CAST,
    EXPR_INDEX,
    EXPR_ASSIGN_INDEX,
    EXPR_ASSIGN_MEMBER,
    EXPR_MEMBER,
    EXPR_SIZEOF,
    EXPR_OFFSETOF,
    EXPR_ALIGNOF,
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
    STMT_STATIC_ASSERT,
    STMT_TYPEDEF,
    STMT_ENUM_DECL,
    STMT_STRUCT_DECL,
    STMT_UNION_DECL,
    STMT_BLOCK
} stmt_kind_t;



#endif /* VC_AST_H */
