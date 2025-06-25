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


static const char *loc_str(char buf[32], regalloc_t *ra, int id, int x64)
{
    if (!ra || id <= 0)
        return "";
    int loc = ra->loc[id];
    if (loc >= 0)
        return regalloc_reg_name(loc);
    if (x64)
        snprintf(buf, 32, "-%d(%%rbp)", -loc * 8);
    else
        snprintf(buf, 32, "-%d(%%ebp)", -loc * 4);
    return buf;
}

static void emit_memory_instr(strbuf_t *sb, ir_instr_t *ins,
                              regalloc_t *ra, int x64);
static void emit_arith_instr(strbuf_t *sb, ir_instr_t *ins,
                             regalloc_t *ra, int x64);
static void emit_branch_instr(strbuf_t *sb, ir_instr_t *ins,
                              regalloc_t *ra, int x64);

static void emit_memory_instr(strbuf_t *sb, ir_instr_t *ins,
                              regalloc_t *ra, int x64)
{
    char buf1[32];
    char buf2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *bp = x64 ? "%rbp" : "%ebp";

    switch (ins->op) {
    case IR_CONST:
        strbuf_appendf(sb, "    mov%s $%d, %s\n", sfx, ins->imm,
                       loc_str(buf1, ra, ins->dest, x64));
        break;
    case IR_LOAD:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ins->name,
                       loc_str(buf1, ra, ins->dest, x64));
        break;
    case IR_STORE:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       ins->name);
        break;
    case IR_LOAD_PARAM: {
        int off = 8 + ins->imm * (x64 ? 8 : 4);
        strbuf_appendf(sb, "    mov%s %d(%s), %s\n", sfx, off, bp,
                       loc_str(buf1, ra, ins->dest, x64));
        break;
    }
    case IR_STORE_PARAM: {
        int off = 8 + ins->imm * (x64 ? 8 : 4);
        strbuf_appendf(sb, "    mov%s %s, %d(%s)\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), off, bp);
        break;
    }
    case IR_ADDR:
        strbuf_appendf(sb, "    mov%s $%s, %s\n", sfx, ins->name,
                       loc_str(buf1, ra, ins->dest, x64));
        break;
    case IR_LOAD_PTR:
        strbuf_appendf(sb, "    mov%s (%s), %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_STORE_PTR:
        strbuf_appendf(sb, "    mov%s %s, (%s)\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->src1, x64));
        break;
    case IR_LOAD_IDX:
        strbuf_appendf(sb, "    mov%s %s(,%s,4), %s\n", sfx,
                       ins->name,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_STORE_IDX:
        strbuf_appendf(sb, "    mov%s %s, %s(,%s,4)\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       ins->name,
                       loc_str(buf2, ra, ins->src1, x64));
        break;
    case IR_ARG:
        strbuf_appendf(sb, "    push%s %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64));
        break;
    case IR_GLOB_STRING:
        strbuf_appendf(sb, "    mov%s $%s, %s\n", sfx, ins->name,
                       loc_str(buf1, ra, ins->dest, x64));
        break;
    case IR_GLOB_VAR:
        /* globals handled separately in data section */
        break;
    case IR_GLOB_ARRAY:
        /* globals handled separately */
        break;
    default:
        break;
    }
}

static void emit_arith_instr(strbuf_t *sb, ir_instr_t *ins,
                             regalloc_t *ra, int x64)
{
    char buf1[32];
    char buf2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = x64 ? "%rax" : "%eax";

    switch (ins->op) {
    case IR_PTR_ADD: {
        int scale = x64 ? 8 : 4;
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    imul%s $%d, %s\n", sfx, scale,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    add%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    case IR_PTR_DIFF: {
        int shift = x64 ? 3 : 2;
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    sub%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    sar%s $%d, %s\n", sfx, shift,
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    case IR_ADD:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    add%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_SUB:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    sub%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_MUL:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    imul%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_DIV:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), ax);
        strbuf_appendf(sb, "    %s\n", x64 ? "cqto" : "cltd");
        strbuf_appendf(sb, "    idiv%s %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64));
        if (ra && ra->loc[ins->dest] >= 0 &&
            strcmp(regalloc_reg_name(ra->loc[ins->dest]), ax) != 0)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                           loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_MOD:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), ax);
        strbuf_appendf(sb, "    %s\n", x64 ? "cqto" : "cltd");
        strbuf_appendf(sb, "    idiv%s %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64));
        if (ra && ra->loc[ins->dest] >= 0 &&
            strcmp(regalloc_reg_name(ra->loc[ins->dest]),
                   x64 ? "%rdx" : "%edx") != 0)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                           x64 ? "%rdx" : "%edx",
                           loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_SHL:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       x64 ? "%rcx" : "%ecx");
        strbuf_appendf(sb, "    sal%s %s, %s\n", sfx, "%cl",
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_SHR:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       x64 ? "%rcx" : "%ecx");
        strbuf_appendf(sb, "    sar%s %s, %s\n", sfx, "%cl",
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_AND:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    and%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_OR:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    or%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_XOR:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    xor%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT: case IR_CMPGT:
    case IR_CMPLE: case IR_CMPGE: {
        const char *cc = "";
        switch (ins->op) {
        case IR_CMPEQ: cc = "e"; break;
        case IR_CMPNE: cc = "ne"; break;
        case IR_CMPLT: cc = "l"; break;
        case IR_CMPGT: cc = "g"; break;
        case IR_CMPLE: cc = "le"; break;
        case IR_CMPGE: cc = "ge"; break;
        default: break;
        }
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    cmp%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    set%s %s\n", cc, "%al");
        strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl",
                       "%al", loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    case IR_LOGAND: {
        int id = label_next_id();
        char fl[32]; char end[32];
        snprintf(fl, sizeof(fl), "L%d_false", id);
        snprintf(end, sizeof(end), "L%d_end", id);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    je %s\n", fl);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    setne %s\n", "%al");
        strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl", "%al",
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    jmp %s\n", end);
        strbuf_appendf(sb, "%s:\n", fl);
        strbuf_appendf(sb, "    mov%s $0, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "%s:\n", end);
        break;
    }
    case IR_LOGOR: {
        int id = label_next_id();
        char tl[32]; char end[32];
        snprintf(tl, sizeof(tl), "L%d_true", id);
        snprintf(end, sizeof(end), "L%d_end", id);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    jne %s\n", tl);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    setne %s\n", "%al");
        strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl", "%al",
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    jmp %s\n", end);
        strbuf_appendf(sb, "%s:\n", tl);
        strbuf_appendf(sb, "    mov%s $1, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "%s:\n", end);
        break;
    }
    default:
        break;
    }
}

static void emit_branch_instr(strbuf_t *sb, ir_instr_t *ins,
                              regalloc_t *ra, int x64)
{
    char buf1[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = x64 ? "%rax" : "%eax";
    const char *bp = x64 ? "%rbp" : "%ebp";
    const char *sp = x64 ? "%rsp" : "%esp";

    switch (ins->op) {
    case IR_RETURN:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), ax);
        strbuf_append(sb, "    ret\n");
        break;
    case IR_CALL:
        strbuf_appendf(sb, "    call %s\n", ins->name);
        if (ins->imm > 0)
            strbuf_appendf(sb, "    add%s $%d, %s\n", sfx,
                           ins->imm * (x64 ? 8 : 4), sp);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                       loc_str(buf1, ra, ins->dest, x64));
        break;
    case IR_FUNC_BEGIN:
        strbuf_appendf(sb, "%s:\n", ins->name);
        strbuf_appendf(sb, "    push%s %s\n", sfx, bp);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, sp, bp);
        break;
    case IR_FUNC_END:
        /* nothing for now */
        break;
    case IR_BR:
        strbuf_appendf(sb, "    jmp %s\n", ins->name);
        break;
    case IR_BCOND:
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64));
        strbuf_appendf(sb, "    je %s\n", ins->name);
        break;
    case IR_LABEL:
        strbuf_appendf(sb, "%s:\n", ins->name);
        break;
    default:
        break;
    }
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
        emit_memory_instr(sb, ins, ra, x64);
        break;

    case IR_PTR_ADD: case IR_PTR_DIFF:
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
            ins->op == IR_GLOB_ARRAY) {
            if (!has_data) {
                fputs(".data\n", out);
                has_data = 1;
            }
            if (ins->src1)
                fprintf(out, ".local %s\n", ins->name);
            fprintf(out, "%s:\n", ins->name);
            if (ins->op == IR_GLOB_VAR) {
                fprintf(out, "    %s %d\n", size_directive, ins->imm);
            } else if (ins->op == IR_GLOB_STRING) {
                fprintf(out, "    .asciz \"%s\"\n", ins->data);
            } else if (ins->op == IR_GLOB_ARRAY) {
                int *vals = (int *)ins->data;
                for (int i = 0; i < ins->imm; i++)
                    fprintf(out, "    %s %d\n", size_directive, vals[i]);
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

