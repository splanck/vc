#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_arith_float.h"
#include "strbuf.h"
#include "regalloc.h"
#include "regalloc_x86.h"

/* Stubs */
int is_intlike(type_kind_t t) { return t == TYPE_INT; }
void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

int main(void) {
    int locs[3] = {0, -1, -2};
    regalloc_set_x86_64(1);
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;

    ins.dest = 2;
    ins.src1 = 1;
    ins.imm = ((unsigned long long)TYPE_INT << 32) | TYPE_INT;

    strbuf_init(&sb);
    emit_cast(&sb, &ins, &ra, 1, ASM_ATT);
    if (!strstr(sb.data, "movq -8(%rbp), %rax") ||
        !strstr(sb.data, "movq %rax, -16(%rbp)")) {
        printf("ATT: unexpected output: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_cast(&sb, &ins, &ra, 1, ASM_INTEL);
    if (!strstr(sb.data, "movq rax, [rbp-8]") ||
        !strstr(sb.data, "movq [rbp-16], rax")) {
        printf("Intel: unexpected output: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    printf("mem2mem cast tests passed\n");
    return 0;
}
