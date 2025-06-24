#ifndef VC_IR_H
#define VC_IR_H

/* IR operation codes */
typedef enum {
    IR_CONST,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_CMPEQ,
    IR_CMPNE,
    IR_CMPLT,
    IR_CMPGT,
    IR_CMPLE,
    IR_CMPGE,
    IR_GLOB_STRING,
    IR_GLOB_VAR,
    IR_LOAD,
    IR_STORE,
    IR_LOAD_PARAM,
    IR_STORE_PARAM,
    IR_ADDR,
    IR_LOAD_PTR,
    IR_STORE_PTR,
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
    int imm;              /* immediate value for constants */
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

void ir_builder_init(ir_builder_t *b);
void ir_builder_free(ir_builder_t *b);

ir_value_t ir_build_const(ir_builder_t *b, int value);
ir_value_t ir_build_load(ir_builder_t *b, const char *name);
ir_value_t ir_build_binop(ir_builder_t *b, ir_op_t op, ir_value_t left, ir_value_t right);
void ir_build_store(ir_builder_t *b, const char *name, ir_value_t val);
ir_value_t ir_build_load_param(ir_builder_t *b, int index);
void ir_build_store_param(ir_builder_t *b, int index, ir_value_t val);
ir_value_t ir_build_addr(ir_builder_t *b, const char *name);
ir_value_t ir_build_load_ptr(ir_builder_t *b, ir_value_t addr);
void ir_build_store_ptr(ir_builder_t *b, ir_value_t addr, ir_value_t val);
void ir_build_return(ir_builder_t *b, ir_value_t val);
void ir_build_arg(ir_builder_t *b, ir_value_t val);
ir_value_t ir_build_call(ir_builder_t *b, const char *name, size_t arg_count);
void ir_build_func_begin(ir_builder_t *b, const char *name);
void ir_build_func_end(ir_builder_t *b);
void ir_build_br(ir_builder_t *b, const char *label);
void ir_build_bcond(ir_builder_t *b, ir_value_t cond, const char *label);
void ir_build_label(ir_builder_t *b, const char *label);
ir_value_t ir_build_string(ir_builder_t *b, const char *data);
void ir_build_glob_var(ir_builder_t *b, const char *name);

#endif /* VC_IR_H */
