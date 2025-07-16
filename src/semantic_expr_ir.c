#include "semantic_expr_ops.h"

/* Mapping from BINOP_* to corresponding IR op. Logical ops use IR_CMPEQ as placeholder. */
static const ir_op_t binop_to_ir[] = {
    [BINOP_ADD]    = IR_ADD,
    [BINOP_SUB]    = IR_SUB,
    [BINOP_MUL]    = IR_MUL,
    [BINOP_DIV]    = IR_DIV,
    [BINOP_MOD]    = IR_MOD,
    [BINOP_SHL]    = IR_SHL,
    [BINOP_SHR]    = IR_SHR,
    [BINOP_BITAND] = IR_AND,
    [BINOP_BITXOR] = IR_XOR,
    [BINOP_BITOR]  = IR_OR,
    [BINOP_EQ]     = IR_CMPEQ,
    [BINOP_NEQ]    = IR_CMPNE,
    [BINOP_LOGAND] = IR_CMPEQ,
    [BINOP_LOGOR]  = IR_CMPEQ,
    [BINOP_LT]     = IR_CMPLT,
    [BINOP_GT]     = IR_CMPGT,
    [BINOP_LE]     = IR_CMPLE,
    [BINOP_GE]     = IR_CMPGE,
};

ir_op_t ir_op_for_binop(binop_t op)
{
    return binop_to_ir[op];
}
