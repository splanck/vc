#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "ir_const.h"

int is_intlike(type_kind_t t) { (void)t; return 0; }

int main(void) {
    ir_builder_t b;
    ir_builder_init(&b);
    const char data[] = {'a','b','\0','c','d'};
    ir_build_string(&b, data, sizeof(data));

    char *buf = NULL;
    size_t sz = 0;
    FILE *f = open_memstream(&buf, &sz);
    if (!f) {
        perror("open_memstream");
        ir_builder_free(&b);
        return 1;
    }
    codegen_emit_x86(f, &b, 0, ASM_ATT);
    fclose(f);
    ir_builder_free(&b);

    int ok = strstr(buf, ".asciz \"ab\\x00cd\"") != NULL;
    if (!ok) {
        printf("unexpected output: %s\n", buf);
        free(buf);
        return 1;
    }
    free(buf);
    printf("glob_string_nul test passed\n");
    return 0;
}
