/*
 * Utilities for printing IR for debugging.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "ir_dump.h"
#include "strbuf.h"

static const char *op_name(ir_op_t op)
{
    switch (op) {
    case IR_CONST: return "IR_CONST";
    case IR_ADD: return "IR_ADD";
    case IR_SUB: return "IR_SUB";
    case IR_MUL: return "IR_MUL";
    case IR_DIV: return "IR_DIV";
    case IR_MOD: return "IR_MOD";
    case IR_SHL: return "IR_SHL";
    case IR_SHR: return "IR_SHR";
    case IR_AND: return "IR_AND";
    case IR_OR: return "IR_OR";
    case IR_XOR: return "IR_XOR";
    case IR_PTR_ADD: return "IR_PTR_ADD";
    case IR_PTR_DIFF: return "IR_PTR_DIFF";
    case IR_CMPEQ: return "IR_CMPEQ";
    case IR_CMPNE: return "IR_CMPNE";
    case IR_CMPLT: return "IR_CMPLT";
    case IR_CMPGT: return "IR_CMPGT";
    case IR_CMPLE: return "IR_CMPLE";
    case IR_CMPGE: return "IR_CMPGE";
    case IR_LOGAND: return "IR_LOGAND";
    case IR_LOGOR: return "IR_LOGOR";
    case IR_GLOB_STRING: return "IR_GLOB_STRING";
    case IR_GLOB_VAR: return "IR_GLOB_VAR";
    case IR_GLOB_ARRAY: return "IR_GLOB_ARRAY";
    case IR_LOAD: return "IR_LOAD";
    case IR_STORE: return "IR_STORE";
    case IR_LOAD_PARAM: return "IR_LOAD_PARAM";
    case IR_STORE_PARAM: return "IR_STORE_PARAM";
    case IR_ADDR: return "IR_ADDR";
    case IR_LOAD_PTR: return "IR_LOAD_PTR";
    case IR_STORE_PTR: return "IR_STORE_PTR";
    case IR_LOAD_IDX: return "IR_LOAD_IDX";
    case IR_STORE_IDX: return "IR_STORE_IDX";
    case IR_ARG: return "IR_ARG";
    case IR_RETURN: return "IR_RETURN";
    case IR_CALL: return "IR_CALL";
    case IR_CALL_IND: return "IR_CALL_IND";
    case IR_FUNC_BEGIN: return "IR_FUNC_BEGIN";
    case IR_FUNC_END: return "IR_FUNC_END";
    case IR_BR: return "IR_BR";
    case IR_BCOND: return "IR_BCOND";
    case IR_LABEL: return "IR_LABEL";
    }
    return "";
}

/*
 * Convert the instruction list into a textual form suitable for
 * debugging. Each line contains the opcode followed by the common
 * fields. Global arrays list the element count instead.
 */
char *ir_to_string(ir_builder_t *ir)
{
    if (!ir)
        return NULL;

    strbuf_t sb;
    strbuf_init(&sb);
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (ins->op == IR_GLOB_ARRAY) {
            strbuf_appendf(&sb, "%s name=%s count=%d\n", op_name(ins->op),
                           ins->name ? ins->name : "", ins->imm);
        } else {
            strbuf_appendf(&sb,
                           "%s dest=%d src1=%d src2=%d imm=%d name=%s data=%s\n",
                           op_name(ins->op), ins->dest, ins->src1, ins->src2,
                           ins->imm, ins->name ? ins->name : "",
                           ins->data ? ins->data : "");
        }
    }
    return sb.data; /* caller frees */
}

