/*
 * Core intermediate representation data types and builders.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_IR_CORE_H
#define VC_IR_CORE_H

#include "ast.h"

/* IR operation codes */
typedef enum {
    IR_CONST,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,
    IR_SHL,
    IR_SHR,
    IR_AND,
    IR_OR,
    IR_XOR,
    IR_CAST,
    IR_FADD,
    IR_FSUB,
    IR_FMUL,
    IR_FDIV,
    IR_LFADD,
    IR_LFSUB,
    IR_LFMUL,
    IR_LFDIV,
    IR_CPLX_CONST,
    IR_CPLX_ADD,
    IR_CPLX_SUB,
    IR_CPLX_MUL,
    IR_CPLX_DIV,
    IR_PTR_ADD,
    IR_PTR_DIFF,
    IR_CMPEQ,
    IR_CMPNE,
    IR_CMPLT,
    IR_CMPGT,
    IR_CMPLE,
    IR_CMPGE,
    IR_LOGAND,
    IR_LOGOR,
    IR_GLOB_STRING,
    IR_GLOB_WSTRING,
    IR_GLOB_VAR,
    IR_GLOB_ARRAY,
    IR_GLOB_UNION,
    IR_GLOB_STRUCT,
    IR_GLOB_ADDR,
    IR_LOAD,
    IR_STORE,
    IR_LOAD_PARAM,
    IR_STORE_PARAM,
    IR_ADDR,
    IR_LOAD_PTR,
    IR_STORE_PTR,
    IR_LOAD_IDX,
    IR_STORE_IDX,
    IR_BFLOAD,
    IR_BFSTORE,
    IR_ALLOCA,
    IR_ARG,
    IR_RETURN,
    IR_RETURN_AGG,
    IR_CALL,
    IR_CALL_PTR,
    IR_CALL_NR,
    IR_CALL_PTR_NR,
    IR_FUNC_BEGIN,
    IR_FUNC_END,
    IR_BR,
    IR_BCOND,
    IR_LABEL
} ir_op_t;

struct ir_instr;

typedef struct {
    int id;
} ir_value_t;

typedef struct ir_instr {
    ir_op_t op;
    int dest;
    int src1;
    int src2;
    long long imm;
    char *name;
    char *data;
    int is_volatile;
    int is_restrict;
    int alias_set;
    type_kind_t type;
    struct ir_instr *next;
    const char *file;
    size_t line;
    size_t column;
} ir_instr_t;


typedef struct alias_ent {
    const char *name;
    int set;
    struct alias_ent *next;
} alias_ent_t;

typedef struct {
    ir_instr_t *head;
    ir_instr_t *tail;
    size_t next_value_id;
    const char *cur_file;
    size_t cur_line;
    size_t cur_column;
    alias_ent_t *aliases;
    int next_alias_id;
} ir_builder_t;

/*
 * Reset the builder and clear any previously emitted instructions. The
 * next value id generated will start at 1.
 */
void ir_builder_init(ir_builder_t *b);

/* Set the location used by subsequently emitted instructions */
void ir_builder_set_loc(ir_builder_t *b, const char *file, size_t line, size_t column);

/* Release all memory owned by the builder, including instruction nodes. */
void ir_builder_free(ir_builder_t *b);

/* Allocate and insert a blank instruction after `pos`. */
ir_instr_t *ir_insert_after(ir_builder_t *b, ir_instr_t *pos);


/* Emit the binary operation `op` with operands `left` and `right`. */
ir_value_t ir_build_binop(ir_builder_t *b, ir_op_t op, ir_value_t left,
                          ir_value_t right, type_kind_t type);

/* Complex arithmetic helpers */
ir_value_t ir_build_cplx_add(ir_builder_t *b, ir_value_t left, ir_value_t right,
                             type_kind_t type);
ir_value_t ir_build_cplx_sub(ir_builder_t *b, ir_value_t left, ir_value_t right,
                             type_kind_t type);
ir_value_t ir_build_cplx_mul(ir_builder_t *b, ir_value_t left, ir_value_t right,
                             type_kind_t type);
ir_value_t ir_build_cplx_div(ir_builder_t *b, ir_value_t left, ir_value_t right,
                             type_kind_t type);

/* Emit IR_LOGAND using `left` and `right`. */
ir_value_t ir_build_logand(ir_builder_t *b, ir_value_t left, ir_value_t right);

/* Emit IR_LOGOR using `left` and `right`. */
ir_value_t ir_build_logor(ir_builder_t *b, ir_value_t left, ir_value_t right);

/* Emit IR_CAST converting `val` from `src_type` to `dst_type`. */
ir_value_t ir_build_cast(ir_builder_t *b, ir_value_t val,
                         type_kind_t src_type, type_kind_t dst_type);

#include "ir_const.h"
#include "ir_memory.h"
#include "ir_control.h"

#endif /* VC_IR_CORE_H */
