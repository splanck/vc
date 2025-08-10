#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_mem.h"
#include "strbuf.h"
#include "regalloc_x86.h"

void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int contains(const char *s, const char *sub) { return strstr(s, sub) != NULL; }

int main(void) {
    int locs[2] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;
    int fail = 0;

    regalloc_set_x86_64(1);

    ins.op = IR_ADDR;
    ins.name = "foo";
    ins.dest = 1;

    /* Destination in register */
    ra.loc[1] = 0;

    strbuf_init(&sb);
    regalloc_set_asm_syntax(ASM_ATT);
    mem_emitters[IR_ADDR](&sb, &ins, &ra, 1, ASM_ATT);
    if (!contains(sb.data, "movabs")) {
        printf("ATT missing movabs: %s\n", sb.data);
        fail = 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    regalloc_set_asm_syntax(ASM_INTEL);
    mem_emitters[IR_ADDR](&sb, &ins, &ra, 1, ASM_INTEL);
    if (!contains(sb.data, "movabs")) {
        printf("Intel missing movabs: %s\n", sb.data);
        fail = 1;
    }
    strbuf_free(&sb);

    /* Destination spilled to stack */
    ra.loc[1] = -1;

    strbuf_init(&sb);
    regalloc_set_asm_syntax(ASM_ATT);
    mem_emitters[IR_ADDR](&sb, &ins, &ra, 1, ASM_ATT);
    if (!contains(sb.data, "movabs") || !contains(sb.data, "movq")) {
        printf("ATT spill failed: %s\n", sb.data);
        fail = 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    regalloc_set_asm_syntax(ASM_INTEL);
    mem_emitters[IR_ADDR](&sb, &ins, &ra, 1, ASM_INTEL);
    if (!contains(sb.data, "movabs") || !contains(sb.data, "movq")) {
        printf("Intel spill failed: %s\n", sb.data);
        fail = 1;
    }
    strbuf_free(&sb);

    if (!fail)
        printf("emit_addr movabs tests passed\n");
    return fail;
}
