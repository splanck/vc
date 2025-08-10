#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_branch.h"
#include "strbuf.h"
#include "regalloc_x86.h"

size_t arg_stack_bytes;
int arg_reg_idx;
int float_reg_idx;
int export_syms;
int dwarf_enabled;

void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int check(const char *out, const char *exp, const char *name) {
    if (strcmp(out, exp) != 0) {
        printf("%s unexpected: %s\n", name, out);
        return 1;
    }
    return 0;
}

int main(void) {
    strbuf_t sb;
    int locs[2] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    int fail = 0;

    regalloc_set_asm_syntax(ASM_ATT);

    /* direct call with hidden stack args but zero arg count */
    ir_instr_t ins = { .op = IR_CALL, .imm = 0, .name = "foo", .type = TYPE_INT, .dest = 0 };
    arg_stack_bytes = 4;
    strbuf_init(&sb);
    emit_branch_instr(&sb, &ins, &ra, 0, ASM_ATT);
    fail |= check(sb.data, "    call foo\n    addl $4, %esp\n", "direct");
    strbuf_free(&sb);

    /* indirect call with alignment adjustment */
    regalloc_set_x86_64(1);
    ra.loc[1] = 2; /* %rcx */
    ins.op = IR_CALL_PTR;
    ins.src1 = 1;
    arg_stack_bytes = 8;
    strbuf_init(&sb);
    emit_branch_instr(&sb, &ins, &ra, 1, ASM_ATT);
    fail |= check(sb.data, "    subq $8, %rsp\n    call *%rcx\n    addq $16, %rsp\n", "indirect");
    strbuf_free(&sb);

    if (!fail)
        printf("call stack cleanup tests passed\n");
    return fail;
}
