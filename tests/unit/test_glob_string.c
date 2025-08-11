#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_mem.h"
#include "strbuf.h"
#include "regalloc.h"
#include "regalloc_x86.h"

void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int contains(const char *s, const char *sub) {
    return strstr(s, sub) != NULL;
}

int main(void) {
    int locs[2] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;

    ra.loc[1] = 0; /* destination register */

    ins.op = IR_GLOB_STRING;
    ins.dest = 1;
    ins.name = "s";

    /* 32-bit emission */
    strbuf_init(&sb);
    emit_memory_instr(&sb, &ins, &ra, 0, ASM_ATT);
    const char *exp_att = "    movl $s, %eax\n";
    if (strcmp(sb.data, exp_att) != 0) {
        printf("glob_string ATT unexpected: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_memory_instr(&sb, &ins, &ra, 0, ASM_INTEL);
    const char *exp_intel = "    movl eax, OFFSET FLAT:s\n";
    if (strcmp(sb.data, exp_intel) != 0) {
        printf("glob_string Intel unexpected: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* 64-bit emission should use movabs */
    regalloc_set_x86_64(1);

    strbuf_init(&sb);
    regalloc_set_asm_syntax(ASM_ATT);
    emit_memory_instr(&sb, &ins, &ra, 1, ASM_ATT);
    if (!contains(sb.data, "movabs")) {
        printf("glob_string x64 ATT missing movabs: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    regalloc_set_asm_syntax(ASM_INTEL);
    emit_memory_instr(&sb, &ins, &ra, 1, ASM_INTEL);
    if (!contains(sb.data, "movabs")) {
        printf("glob_string x64 Intel missing movabs: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    printf("glob_string test passed\n");
    return 0;
}
