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
    int locs[2] = {0, -1};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins = { .op = IR_RETURN, .src1 = 1, .type = TYPE_DOUBLE };
    int fail = 0;

    strbuf_init(&sb);
    regalloc_set_x86_64(1);

    regalloc_set_asm_syntax(ASM_ATT);
    emit_branch_instr(&sb, &ins, &ra, 1, ASM_ATT);
    fail |= check(sb.data, "    movsd -8(%rbp), %xmm0\n    ret\n", "ATT");
    sb.len = 0; if (sb.data) sb.data[0] = '\0';

    regalloc_set_asm_syntax(ASM_INTEL);
    emit_branch_instr(&sb, &ins, &ra, 1, ASM_INTEL);
    fail |= check(sb.data, "    movsd xmm0, [rbp-8]\n    ret\n", "Intel");

    strbuf_free(&sb);
    if (!fail) printf("return double tests passed\n");
    return fail;
}
