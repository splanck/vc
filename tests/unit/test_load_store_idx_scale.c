#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "codegen_loadstore.h"
#include "strbuf.h"
#include "regalloc.h"

/* Provide minimal stubs required by codegen helpers. */
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

    ra.loc[1] = 0; /* index register */
    ra.loc[2] = 1; /* dest/value register */
    ins.op = IR_LOAD_IDX;
    ins.dest = 2;
    ins.src1 = 1;
    ins.name = "base";
    ins.type = TYPE_PTR;
    ins.imm = 0;

    /* 32-bit load */
    strbuf_init(&sb);
    emit_load_idx(&sb, &ins, &ra, 0, ASM_ATT);
    if (!strstr(sb.data, ",4)")) {
        printf("load idx 32 ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_load_idx(&sb, &ins, &ra, 0, ASM_INTEL);
    if (!strstr(sb.data, ",4)")) {
        printf("load idx 32 Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* 32-bit store */
    ins.op = IR_STORE_IDX;
    ins.src2 = 2;
    strbuf_init(&sb);
    emit_store_idx(&sb, &ins, &ra, 0, ASM_ATT);
    if (!strstr(sb.data, ",4)")) {
        printf("store idx 32 ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_store_idx(&sb, &ins, &ra, 0, ASM_INTEL);
    if (!strstr(sb.data, ",4)")) {
        printf("store idx 32 Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* 64-bit load */
    strbuf_init(&sb);
    emit_load_idx(&sb, &ins, &ra, 1, ASM_ATT);
    if (!strstr(sb.data, ",8)")) {
        printf("load idx 64 ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_load_idx(&sb, &ins, &ra, 1, ASM_INTEL);
    if (!strstr(sb.data, ",8)")) {
        printf("load idx 64 Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* 64-bit store */
    strbuf_init(&sb);
    emit_store_idx(&sb, &ins, &ra, 1, ASM_ATT);
    if (!strstr(sb.data, ",8)")) {
        printf("store idx 64 ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_store_idx(&sb, &ins, &ra, 1, ASM_INTEL);
    if (!strstr(sb.data, ",8)")) {
        printf("store idx 64 Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    printf("load/store idx scale tests passed\n");
    return 0;
}
