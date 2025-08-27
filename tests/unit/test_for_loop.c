#include <stdio.h>
#include <string.h>
#include "token.h"
#include "parser.h"
#include "parser_core.h"
#include "semantic_global.h"
#include "symtable.h"
#include "ir_core.h"
#include "ast.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void test_for_loop_ir_order(void)
{
    const char *src = "int f(void){ for (int i = 0; i < 3; i++) {} return 0; }";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    func_t *fn = parser_parse_func(&p, NULL, 0, 0);
    ASSERT(fn);

    symtable_t funcs, globals;
    symtable_init(&funcs); symtable_init(&globals);
    ir_builder_t ir; ir_builder_init(&ir);
    int ok = emit_func_ir(fn, &funcs, &globals, &ir);
    ASSERT(ok);

    int idx = 0;
    int idx_func_begin = -1;
    int idx_start = -1;
    int idx_bcond = -1;
    int idx_cont = -1;
    int idx_br = -1;
    int idx_end = -1;
    for (ir_instr_t *it = ir.head; it; it = it->next, idx++) {
        if (it->op == IR_FUNC_BEGIN)
            idx_func_begin = idx;
        else if (it->op == IR_LABEL && it->name && strstr(it->name, "_start"))
            idx_start = idx;
        else if (it->op == IR_BCOND)
            idx_bcond = idx;
        else if (it->op == IR_LABEL && it->name && strstr(it->name, "_cont"))
            idx_cont = idx;
        else if (it->op == IR_BR && it->name && strstr(it->name, "_start"))
            idx_br = idx;
        else if (it->op == IR_LABEL && it->name && strstr(it->name, "_end"))
            idx_end = idx;
    }
    ASSERT(idx_func_begin >= 0);
    ASSERT(idx_start > idx_func_begin);
    ASSERT(idx_bcond > idx_start);
    ASSERT(idx_cont > idx_bcond);
    ASSERT(idx_br > idx_cont);
    ASSERT(idx_end > idx_br);
    ASSERT(idx_br - idx_cont >= 2);

    ir_builder_free(&ir);
    ast_free_func(fn);
    lexer_free_tokens(toks, count);
    symtable_free(&funcs); symtable_free(&globals);
}

int main(void)
{
    test_for_loop_ir_order();
    if (failures == 0)
        printf("All for_loop tests passed\n");
    else
        printf("%d for_loop test(s) failed\n", failures);
    return failures ? 1 : 0;
}
