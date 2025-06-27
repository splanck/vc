/*
 * Core intermediate representation data types and builders.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_IR_CORE_H
#define VC_IR_CORE_H

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
    IR_FADD,
    IR_FSUB,
    IR_FMUL,
    IR_FDIV,
    IR_LFADD,
    IR_LFSUB,
    IR_LFMUL,
    IR_LFDIV,
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
    IR_GLOB_VAR,
    IR_GLOB_ARRAY,
    IR_GLOB_UNION,
    IR_GLOB_STRUCT,
    IR_LOAD,
    IR_STORE,
    IR_LOAD_PARAM,
    IR_STORE_PARAM,
    IR_ADDR,
    IR_LOAD_PTR,
    IR_STORE_PTR,
    IR_LOAD_IDX,
    IR_STORE_IDX,
    IR_ALLOCA,
    IR_ARG,
    IR_RETURN,
    IR_CALL,
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
    struct ir_instr *next;
} ir_instr_t;

typedef struct {
    ir_instr_t *head;
    ir_instr_t *tail;
    int next_value_id;
} ir_builder_t;

void ir_builder_init(ir_builder_t *b);
void ir_builder_free(ir_builder_t *b);

ir_value_t ir_build_const(ir_builder_t *b, long long value);
ir_value_t ir_build_load(ir_builder_t *b, const char *name);
ir_value_t ir_build_load_vol(ir_builder_t *b, const char *name);

ir_value_t ir_build_binop(ir_builder_t *b, ir_op_t op, ir_value_t left,
                          ir_value_t right);
ir_value_t ir_build_logand(ir_builder_t *b, ir_value_t left, ir_value_t right);
ir_value_t ir_build_logor(ir_builder_t *b, ir_value_t left, ir_value_t right);

void ir_build_store(ir_builder_t *b, const char *name, ir_value_t val);
void ir_build_store_vol(ir_builder_t *b, const char *name, ir_value_t val);

ir_value_t ir_build_load_param(ir_builder_t *b, int index);
void ir_build_store_param(ir_builder_t *b, int index, ir_value_t val);

ir_value_t ir_build_addr(ir_builder_t *b, const char *name);
ir_value_t ir_build_load_ptr(ir_builder_t *b, ir_value_t addr);
void ir_build_store_ptr(ir_builder_t *b, ir_value_t addr, ir_value_t val);

ir_value_t ir_build_ptr_add(ir_builder_t *b, ir_value_t ptr, ir_value_t idx,
                            int elem_size);
ir_value_t ir_build_ptr_diff(ir_builder_t *b, ir_value_t a, ir_value_t bptr,
                             int elem_size);

ir_value_t ir_build_load_idx(ir_builder_t *b, const char *name, ir_value_t idx);
ir_value_t ir_build_load_idx_vol(ir_builder_t *b, const char *name,
                                 ir_value_t idx);
void ir_build_store_idx(ir_builder_t *b, const char *name, ir_value_t idx,
                        ir_value_t val);
void ir_build_store_idx_vol(ir_builder_t *b, const char *name, ir_value_t idx,
                            ir_value_t val);

ir_value_t ir_build_alloca(ir_builder_t *b, ir_value_t size);

void ir_build_return(ir_builder_t *b, ir_value_t val);
void ir_build_arg(ir_builder_t *b, ir_value_t val);
ir_value_t ir_build_call(ir_builder_t *b, const char *name, size_t arg_count);

void ir_build_func_begin(ir_builder_t *b, const char *name);
void ir_build_func_end(ir_builder_t *b);

void ir_build_br(ir_builder_t *b, const char *label);
void ir_build_bcond(ir_builder_t *b, ir_value_t cond, const char *label);
void ir_build_label(ir_builder_t *b, const char *label);

ir_value_t ir_build_string(ir_builder_t *b, const char *data);

#endif /* VC_IR_CORE_H */
