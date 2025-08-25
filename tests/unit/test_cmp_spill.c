#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_arith_int.h"
#include "strbuf.h"
#include "regalloc.h"

/* Stubs required by codegen_arith_int.c */
const char *label_format_suffix(char buf[32], const char *prefix, int id,
                                const char *suffix) { (void)buf; (void)prefix; (void)id; (void)suffix; return ""; }
void emit_float_binop(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                      asm_syntax_t syntax) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; }
void emit_long_float_binop(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra,
                           int x64, asm_syntax_t syntax) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; }
void emit_cplx_addsub(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                      asm_syntax_t syntax, const char *op) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; (void)op; }
void emit_cplx_mul(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                   asm_syntax_t syntax) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; }
void emit_cplx_div(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                   asm_syntax_t syntax) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; }
void emit_cast(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
               asm_syntax_t syntax) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; }
int label_next_id(void) { return 0; }
void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int contains(const char *s, const char *sub) { return strstr(s, sub) != NULL; }

int main(void) {
    int locs[4] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;

    ins.op = IR_CMPEQ;
    ins.src1 = 1;
    ins.src2 = 2;
    ins.dest = 3;
    ins.type = TYPE_INT;

    /* Register destination */
    ra.loc[1] = 0; /* %eax */
    ra.loc[2] = 1; /* %ebx */
    ra.loc[3] = 2; /* %ecx */

    strbuf_init(&sb);
    emit_cmp(&sb, &ins, &ra, 0, ASM_ATT);
    if (!contains(sb.data, "movzbl %al, %ecx") || contains(sb.data, "movb")) {
        printf("register ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* Spilled destination */
    ra.loc[3] = -1; /* stack slot */

    strbuf_init(&sb);
    emit_cmp(&sb, &ins, &ra, 0, ASM_ATT);
    const char *out = sb.data;
    if (!contains(out, "movb %al, -4(%ebp)") ||
        !contains(out, "movzbl %al, %eax") ||
        !contains(out, "movl %eax, -4(%ebp)") ||
        contains(out, "movzbl %al, -4(%ebp)")) {
        printf("spill ATT failed: %s\n", out);
        return 1;
    }
    strbuf_free(&sb);

    /* Both operands spilled, register destination */
    ra.loc[1] = -1; /* stack slot */
    ra.loc[2] = -2; /* stack slot */
    ra.loc[3] = 2;  /* %ecx */

    strbuf_init(&sb);
    emit_cmp(&sb, &ins, &ra, 0, ASM_ATT);
    out = sb.data;
    if (!contains(out, "movl -4(%ebp), %eax") ||
        !contains(out, "cmpl -8(%ebp), %eax") ||
        contains(out, "cmpl -8(%ebp), -4(%ebp)")) {
        printf("both spill ATT reg dest failed: %s\n", out);
        return 1;
    }
    strbuf_free(&sb);

    /* Both operands spilled, spilled destination */
    ra.loc[3] = -3; /* stack slot */

    strbuf_init(&sb);
    emit_cmp(&sb, &ins, &ra, 0, ASM_ATT);
    out = sb.data;
    if (!contains(out, "movl -4(%ebp), %eax") ||
        !contains(out, "cmpl -8(%ebp), %eax") ||
        !contains(out, "movb %al, -12(%ebp)") ||
        !contains(out, "movzbl %al, %eax") ||
        !contains(out, "movl %eax, -12(%ebp)") ||
        contains(out, "cmpl -8(%ebp), -4(%ebp)")) {
        printf("both spill ATT spill dest failed: %s\n", out);
        return 1;
    }
    strbuf_free(&sb);

    printf("emit_cmp tests passed\n");
    return 0;
}
