#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "codegen.h"
#include "regalloc.h"
#include "regalloc_x86.h"
#include "strbuf.h"


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

static void emit_instr(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64)
{
    char buf1[32];
    char buf2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = x64 ? "%rax" : "%eax";
    const char *bp = x64 ? "%rbp" : "%ebp";
    const char *sp = x64 ? "%rsp" : "%esp";
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
    }
}

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

