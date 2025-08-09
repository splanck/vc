#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "codegen_loadstore.h"
#include "strbuf.h"
#include "regalloc.h"

/* Provide minimal stubs to satisfy linker requirements. */
const char *fmt_stack(char buf[32], const char *name, int x64,
                      asm_syntax_t syntax) {
    (void)buf; (void)x64; (void)syntax;
    return name;
}

void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

int main(void) {
    int locs[3] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;

    ra.loc[1] = -1; /* pointer in stack slot */
    ra.loc[2] = 1;  /* destination register %ebx */
    ins.op = IR_LOAD_PTR;
    ins.dest = 2;
    ins.src1 = 1;
    ins.type = TYPE_INT;

    strbuf_init(&sb);
    emit_load_ptr(&sb, &ins, &ra, 0, ASM_ATT);
    const char *exp_att = "    movl -4(%ebp), %eax\n    movl (%eax), %ebx\n";
    if (strcmp(sb.data, exp_att) != 0) {
        printf("load ATT unexpected: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_load_ptr(&sb, &ins, &ra, 0, ASM_INTEL);
    const char *exp_intel = "    movl eax, [ebp-4]\n    movl ebx, [eax]\n";
    if (strcmp(sb.data, exp_intel) != 0) {
        printf("load Intel unexpected: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    printf("load ptr stack test passed\n");
    return 0;
}
