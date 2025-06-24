#ifndef VC_IR_H
#define VC_IR_H

/* IR operation codes */
typedef enum {
    IR_CONST,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_LOAD,
    IR_STORE,
    IR_RETURN,
    IR_CALL,
    IR_FUNC_BEGIN,
    IR_FUNC_END,
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
    char *name;           /* identifier for loads */
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
void ir_build_return(ir_builder_t *b, ir_value_t val);
ir_value_t ir_build_call(ir_builder_t *b, const char *name);
void ir_build_func_begin(ir_builder_t *b, const char *name);
void ir_build_func_end(ir_builder_t *b);
void ir_build_bcond(ir_builder_t *b, ir_value_t cond, const char *label);
void ir_build_label(ir_builder_t *b, const char *label);

#endif /* VC_IR_H */
