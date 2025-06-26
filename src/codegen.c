/*
 * Generate x86 assembly from IR.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "codegen.h"
#include "regalloc.h"
#include "regalloc_x86.h"
#include "strbuf.h"
#include "label.h"
#include "codegen_mem.h"
#include "codegen_arith.h"
#include "codegen_branch.h"

int export_syms = 0;

void codegen_set_export(int flag)
{
    export_syms = flag;
}



static void emit_instr(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64)
{
    switch (ins->op) {
    case IR_CONST: case IR_LOAD: case IR_STORE:
    case IR_LOAD_PARAM: case IR_STORE_PARAM:
    case IR_ADDR: case IR_LOAD_PTR: case IR_STORE_PTR:
    case IR_LOAD_IDX: case IR_STORE_IDX:
    case IR_ARG: case IR_GLOB_STRING:
    case IR_GLOB_VAR: case IR_GLOB_ARRAY:
    case IR_GLOB_UNION: case IR_GLOB_STRUCT:
        emit_memory_instr(sb, ins, ra, x64);
        break;

    case IR_PTR_ADD: case IR_PTR_DIFF:
    case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
    case IR_LFADD: case IR_LFSUB: case IR_LFMUL: case IR_LFDIV:
    case IR_ADD: case IR_SUB: case IR_MUL:
    case IR_DIV: case IR_MOD: case IR_SHL:
    case IR_SHR: case IR_AND: case IR_OR:
    case IR_XOR:
    case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT: case IR_CMPGT:
    case IR_CMPLE: case IR_CMPGE:
    case IR_LOGAND: case IR_LOGOR:
        emit_arith_instr(sb, ins, ra, x64);
        break;

    default:
        emit_branch_instr(sb, ins, ra, x64);
        break;
    }
}

/*
 * Translate the IR instruction stream to x86 and return it as a string.
 *
 * Register allocation is performed and each instruction is lowered by
 * `emit_instr`. Global data directives are not included. The returned
 * buffer is heap allocated and must be freed by the caller.
 */
char *codegen_ir_to_string(ir_builder_t *ir, int x64)
{
    if (!ir)
        return NULL;
    regalloc_t ra;
    regalloc_set_x86_64(x64);
    regalloc_run(ir, &ra);

    strbuf_t sb;
    strbuf_init(&sb);
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next)
        emit_instr(&sb, ins, &ra, x64);

    regalloc_free(&ra);
    return sb.data; /* caller takes ownership */
}

/*
 * Emit the assembly representation of `ir` to the stream `out`.
 *
 * Global declarations are written first using the appropriate `.data`
 * directives. The remaining instructions are lowered via
 * `codegen_ir_to_string` and printed after an optional `.text` header.
 * Set `x64` to enable 64-bit output.
 */
void codegen_emit_x86(FILE *out, ir_builder_t *ir, int x64)
{
    if (!out)
        return;
    const char *size_directive = x64 ? ".quad" : ".long";
    int has_data = 0;
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (ins->op == IR_GLOB_VAR || ins->op == IR_GLOB_STRING ||
            ins->op == IR_GLOB_ARRAY || ins->op == IR_GLOB_UNION ||
            ins->op == IR_GLOB_STRUCT) {
            if (!has_data) {
                fputs(".data\n", out);
                has_data = 1;
            }
            if (ins->src1)
                fprintf(out, ".local %s\n", ins->name);
            fprintf(out, "%s:\n", ins->name);
            if (ins->op == IR_GLOB_VAR) {
                fprintf(out, "    %s %lld\n", size_directive, ins->imm);
            } else if (ins->op == IR_GLOB_STRING) {
                fprintf(out, "    .asciz \"%s\"\n", ins->data);
            } else if (ins->op == IR_GLOB_ARRAY) {
                long long *vals = (long long *)ins->data;
                for (long long i = 0; i < ins->imm; i++)
                    fprintf(out, "    %s %lld\n", size_directive, vals[i]);
            } else if (ins->op == IR_GLOB_UNION || ins->op == IR_GLOB_STRUCT) {
                fprintf(out, "    .zero %lld\n", ins->imm);
            }
        }
    }
    if (has_data)
        fputs(".text\n", out);

    char *text = codegen_ir_to_string(ir, x64);
    if (text) {
        fputs(text, out);
        free(text);
    }
}

