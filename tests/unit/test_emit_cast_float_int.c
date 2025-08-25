#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "codegen_arith_float.h"
#include "strbuf.h"
#include "regalloc_x86.h"

int is_intlike(type_kind_t t) {
    switch (t) {
    case TYPE_INT: case TYPE_UINT: case TYPE_CHAR: case TYPE_UCHAR:
    case TYPE_SHORT: case TYPE_USHORT: case TYPE_LONG: case TYPE_ULONG:
    case TYPE_LLONG: case TYPE_ULLONG: case TYPE_BOOL:
        return 1;
    default:
        return 0;
    }
}

void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int check(const char *got, const char *expect, const char *msg) {
    if (!strstr(got, expect)) {
        printf("%s failed: %s\n", msg, got);
        return 1;
    }
    return 0;
}

int main(void) {
    ir_instr_t ins = {0};
    strbuf_t sb;
    regalloc_t ra;
    int locs[3] = {0};
    int fail = 0;

    regalloc_xmm_reset();

    ra.loc = locs;
    ra.stack_slots = 0;
    ra.loc[1] = 0; /* dest register index */
    ins.dest = 1;

    strbuf_init(&sb);

    /* 32-bit targets */
    regalloc_set_x86_64(0);
    ins.src1 = 0;

    ins.imm = ((long long)TYPE_FLOAT << 32) | TYPE_INT;
    emit_cast(&sb, &ins, &ra, 0, ASM_ATT);
    fail |= check(sb.data, "cvttss2si %xmm0, %eax", "float->int ATT");
    sb.len = 0; sb.data[0] = '\0';
    emit_cast(&sb, &ins, &ra, 0, ASM_INTEL);
    fail |= check(sb.data, "cvttss2si eax, xmm0", "float->int Intel");
    sb.len = 0; sb.data[0] = '\0';

    ins.imm = ((long long)TYPE_FLOAT << 32) | TYPE_UINT;
    emit_cast(&sb, &ins, &ra, 0, ASM_ATT);
    fail |= check(sb.data, "cvttss2si %xmm0, %eax", "float->uint ATT");
    sb.len = 0; sb.data[0] = '\0';
    emit_cast(&sb, &ins, &ra, 0, ASM_INTEL);
    fail |= check(sb.data, "cvttss2si eax, xmm0", "float->uint Intel");
    sb.len = 0; sb.data[0] = '\0';

    ins.imm = ((long long)TYPE_DOUBLE << 32) | TYPE_INT;
    emit_cast(&sb, &ins, &ra, 0, ASM_ATT);
    fail |= check(sb.data, "cvttsd2si %xmm0, %eax", "double->int ATT");
    sb.len = 0; sb.data[0] = '\0';
    emit_cast(&sb, &ins, &ra, 0, ASM_INTEL);
    fail |= check(sb.data, "cvttsd2si eax, xmm0", "double->int Intel");
    sb.len = 0; sb.data[0] = '\0';

    ins.imm = ((long long)TYPE_DOUBLE << 32) | TYPE_UINT;
    emit_cast(&sb, &ins, &ra, 0, ASM_ATT);
    fail |= check(sb.data, "cvttsd2si %xmm0, %eax", "double->uint ATT");
    sb.len = 0; sb.data[0] = '\0';
    emit_cast(&sb, &ins, &ra, 0, ASM_INTEL);
    fail |= check(sb.data, "cvttsd2si eax, xmm0", "double->uint Intel");
    sb.len = 0; sb.data[0] = '\0';

    /* 64-bit targets */
    regalloc_set_x86_64(1);
    ins.imm = ((long long)TYPE_FLOAT << 32) | TYPE_LLONG;
    emit_cast(&sb, &ins, &ra, 1, ASM_ATT);
    fail |= check(sb.data, "cvttss2siq %xmm0, %rax", "float->llong ATT");
    sb.len = 0; sb.data[0] = '\0';
    emit_cast(&sb, &ins, &ra, 1, ASM_INTEL);
    fail |= check(sb.data, "cvttss2siq rax, xmm0", "float->llong Intel");
    sb.len = 0; sb.data[0] = '\0';

    ins.imm = ((long long)TYPE_FLOAT << 32) | TYPE_ULLONG;
    emit_cast(&sb, &ins, &ra, 1, ASM_ATT);
    fail |= check(sb.data, "cvttss2siq %xmm0, %rax", "float->ullong ATT");
    sb.len = 0; sb.data[0] = '\0';
    emit_cast(&sb, &ins, &ra, 1, ASM_INTEL);
    fail |= check(sb.data, "cvttss2siq rax, xmm0", "float->ullong Intel");
    sb.len = 0; sb.data[0] = '\0';

    ins.imm = ((long long)TYPE_DOUBLE << 32) | TYPE_LLONG;
    emit_cast(&sb, &ins, &ra, 1, ASM_ATT);
    fail |= check(sb.data, "cvttsd2siq %xmm0, %rax", "double->llong ATT");
    sb.len = 0; sb.data[0] = '\0';
    emit_cast(&sb, &ins, &ra, 1, ASM_INTEL);
    fail |= check(sb.data, "cvttsd2siq rax, xmm0", "double->llong Intel");
    sb.len = 0; sb.data[0] = '\0';

    ins.imm = ((long long)TYPE_DOUBLE << 32) | TYPE_ULLONG;
    emit_cast(&sb, &ins, &ra, 1, ASM_ATT);
    fail |= check(sb.data, "cvttsd2siq %xmm0, %rax", "double->ullong ATT");
    sb.len = 0; sb.data[0] = '\0';
    emit_cast(&sb, &ins, &ra, 1, ASM_INTEL);
    fail |= check(sb.data, "cvttsd2siq rax, xmm0", "double->ullong Intel");

    strbuf_free(&sb);
    if (fail) return 1;
    printf("emit_cast float-int tests passed\n");
    return 0;
}
