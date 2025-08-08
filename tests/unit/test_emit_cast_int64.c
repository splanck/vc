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

int main(void) {
    ir_instr_t ins = {0};
    strbuf_t sb;
    int fail = 0;

    strbuf_init(&sb);

    ins.imm = ((long long)TYPE_LLONG << 32) | TYPE_FLOAT;
    emit_cast(&sb, &ins, NULL, 1, ASM_ATT);
    if (!strstr(sb.data, "cvtsi2ssq")) {
        printf("int64->float ATT failed: %s\n", sb.data);
        fail = 1;
    }
    sb.len = 0; if (sb.data) sb.data[0] = '\0';
    emit_cast(&sb, &ins, NULL, 1, ASM_INTEL);
    if (!strstr(sb.data, "cvtsi2ssq")) {
        printf("int64->float Intel failed: %s\n", sb.data);
        fail = 1;
    }
    sb.len = 0; if (sb.data) sb.data[0] = '\0';

    ins.imm = ((long long)TYPE_LLONG << 32) | TYPE_DOUBLE;
    emit_cast(&sb, &ins, NULL, 1, ASM_ATT);
    if (!strstr(sb.data, "cvtsi2sdq")) {
        printf("int64->double ATT failed: %s\n", sb.data);
        fail = 1;
    }
    sb.len = 0; if (sb.data) sb.data[0] = '\0';
    emit_cast(&sb, &ins, NULL, 1, ASM_INTEL);
    if (!strstr(sb.data, "cvtsi2sdq")) {
        printf("int64->double Intel failed: %s\n", sb.data);
        fail = 1;
    }
    sb.len = 0; if (sb.data) sb.data[0] = '\0';

    ins.imm = ((long long)TYPE_FLOAT << 32) | TYPE_LLONG;
    emit_cast(&sb, &ins, NULL, 1, ASM_ATT);
    if (!strstr(sb.data, "cvttss2siq")) {
        printf("float->int64 ATT failed: %s\n", sb.data);
        fail = 1;
    }
    sb.len = 0; if (sb.data) sb.data[0] = '\0';
    emit_cast(&sb, &ins, NULL, 1, ASM_INTEL);
    if (!strstr(sb.data, "cvttss2siq")) {
        printf("float->int64 Intel failed: %s\n", sb.data);
        fail = 1;
    }
    sb.len = 0; if (sb.data) sb.data[0] = '\0';

    ins.imm = ((long long)TYPE_DOUBLE << 32) | TYPE_LLONG;
    emit_cast(&sb, &ins, NULL, 1, ASM_ATT);
    if (!strstr(sb.data, "cvttsd2siq")) {
        printf("double->int64 ATT failed: %s\n", sb.data);
        fail = 1;
    }
    sb.len = 0; if (sb.data) sb.data[0] = '\0';
    emit_cast(&sb, &ins, NULL, 1, ASM_INTEL);
    if (!strstr(sb.data, "cvttsd2siq")) {
        printf("double->int64 Intel failed: %s\n", sb.data);
        fail = 1;
    }

    strbuf_free(&sb);
    if (fail)
        return 1;
    printf("emit_cast int64 tests passed\n");
    return 0;
}
