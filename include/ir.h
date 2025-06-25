/*
 * Intermediate representation data types.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_IR_H
#define VC_IR_H

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
    IR_LOAD,
    IR_STORE,
    IR_LOAD_PARAM,
    IR_STORE_PARAM,
    IR_ADDR,
    IR_LOAD_PTR,
    IR_STORE_PTR,
    IR_LOAD_IDX,
    IR_STORE_IDX,
    IR_ARG,
    IR_RETURN,
    IR_CALL,
    IR_FUNC_BEGIN,
    IR_FUNC_END,
    IR_BR,
    IR_BCOND,
    IR_LABEL
} ir_op_t;

/* Forward declaration */
struct ir_instr;

/* Value produced by an instruction */
typedef struct {
    int id; /* unique value id */
} ir_value_t;

/* IR instruction representation */
typedef struct ir_instr {
    ir_op_t op;
    int dest;             /* destination value id (-1 if none) */
    int src1;             /* first operand */
    int src2;             /* second operand */
    long long imm;        /* immediate value for constants and sizes */
    char *name;           /* identifier / label */
    char *data;           /* for string literals */
    struct ir_instr *next;
} ir_instr_t;

/* IR builder accumulates instructions sequentially */
typedef struct {
    ir_instr_t *head;
    ir_instr_t *tail;
    int next_value_id;
} ir_builder_t;

/* Initialize an IR builder. */
void ir_builder_init(ir_builder_t *b);

/* Free all instructions accumulated in the builder. */
void ir_builder_free(ir_builder_t *b);

/* Append IR_CONST producing a new value holding an immediate. */
ir_value_t ir_build_const(ir_builder_t *b, long long value);

/* Append IR_LOAD for variable `name`. */
ir_value_t ir_build_load(ir_builder_t *b, const char *name);

/* Append a binary arithmetic or comparison op. */
ir_value_t ir_build_binop(ir_builder_t *b, ir_op_t op, ir_value_t left, ir_value_t right);
ir_value_t ir_build_logand(ir_builder_t *b, ir_value_t left, ir_value_t right);
ir_value_t ir_build_logor(ir_builder_t *b, ir_value_t left, ir_value_t right);

/* Append IR_STORE assigning `val` to variable `name`. */
void ir_build_store(ir_builder_t *b, const char *name, ir_value_t val);

/* Append IR_LOAD_PARAM reading argument `index`. */
ir_value_t ir_build_load_param(ir_builder_t *b, int index);

/* Append IR_STORE_PARAM writing `val` to parameter `index`. */
void ir_build_store_param(ir_builder_t *b, int index, ir_value_t val);

/* Append IR_ADDR that yields the address of `name`. */
ir_value_t ir_build_addr(ir_builder_t *b, const char *name);

/* Append IR_LOAD_PTR that loads from pointer value `addr`. */
ir_value_t ir_build_load_ptr(ir_builder_t *b, ir_value_t addr);

/* Append IR_STORE_PTR that stores `val` through pointer `addr`. */
void ir_build_store_ptr(ir_builder_t *b, ir_value_t addr, ir_value_t val);

/* Pointer arithmetic helpers */
ir_value_t ir_build_ptr_add(ir_builder_t *b, ir_value_t ptr, ir_value_t idx,
                            int elem_size);
ir_value_t ir_build_ptr_diff(ir_builder_t *b, ir_value_t a, ir_value_t bptr,
                             int elem_size);

/* Append IR_LOAD_IDX fetching `name[idx]`. */
ir_value_t ir_build_load_idx(ir_builder_t *b, const char *name, ir_value_t idx);

/* Append IR_STORE_IDX setting `name[idx] = val`. */
void ir_build_store_idx(ir_builder_t *b, const char *name, ir_value_t idx,
                        ir_value_t val);

/* Append IR_RETURN with return value `val`. */
void ir_build_return(ir_builder_t *b, ir_value_t val);

/* Push an argument for the next IR_CALL. */
void ir_build_arg(ir_builder_t *b, ir_value_t val);

/* Append IR_CALL to `name` expecting `arg_count` preceding IR_ARGs. */
ir_value_t ir_build_call(ir_builder_t *b, const char *name, size_t arg_count);

/* Mark start and end of a function. */
void ir_build_func_begin(ir_builder_t *b, const char *name);
void ir_build_func_end(ir_builder_t *b);

/* Unconditional/conditional branches and labels. */
void ir_build_br(ir_builder_t *b, const char *label);
void ir_build_bcond(ir_builder_t *b, ir_value_t cond, const char *label);
void ir_build_label(ir_builder_t *b, const char *label);

/* Global data declarations. */
ir_value_t ir_build_string(ir_builder_t *b, const char *data);
void ir_build_glob_var(ir_builder_t *b, const char *name, long long value,
                       int is_static);
void ir_build_glob_array(ir_builder_t *b, const char *name,
                         const long long *values, size_t count, int is_static);
void ir_build_glob_union(ir_builder_t *b, const char *name, int size,
                         int is_static);

#endif /* VC_IR_H */
