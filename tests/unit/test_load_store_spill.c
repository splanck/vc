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

static int has_invalid(const char *s) {
    return strstr(s, "[[") || strstr(s, "((");
}

int main(void) {
    int locs[3] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;

    /* Test load from spilled address */
    ra.loc[1] = -1; /* address in stack slot */
    ra.loc[2] = 0;  /* destination register */
    ins.op = IR_LOAD_PTR;
    ins.dest = 2;
    ins.src1 = 1;
    ins.type = TYPE_INT;

    strbuf_init(&sb);
    emit_load_ptr(&sb, &ins, &ra, 0, ASM_ATT);
    if (has_invalid(sb.data)) {
        printf("load ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_load_ptr(&sb, &ins, &ra, 0, ASM_INTEL);
    if (has_invalid(sb.data)) {
        printf("load Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* Test store to spilled address */
    ra.loc[1] = -1; /* address in stack slot */
    ra.loc[2] = 0;  /* value register */
    ins.op = IR_STORE_PTR;
    ins.src1 = 1;
    ins.src2 = 2;
    ins.type = TYPE_INT;

    strbuf_init(&sb);
    emit_store_ptr(&sb, &ins, &ra, 0, ASM_ATT);
    if (has_invalid(sb.data)) {
        printf("store ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_store_ptr(&sb, &ins, &ra, 0, ASM_INTEL);
    if (has_invalid(sb.data)) {
        printf("store Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    printf("load/store spill tests passed\n");
    return 0;
}

