#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "ir_core.h"

/* forward decl not in header */
void fold_constants(ir_builder_t *ir);

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

extern void *malloc(size_t size); /* real malloc */
extern void *calloc(size_t nmemb, size_t size); /* real calloc */
extern void free(void *ptr); /* real free */
static int fail_alloc = 0;
static int fail_after = 0;
static int allocs = 0;

void *test_malloc(size_t size)
{
    if (fail_alloc) {
        if (fail_after <= 0)
            return NULL;
        fail_after--;
    }
    void *p = malloc(size);
    if (p)
        allocs++;
    return p;
}

void *test_calloc(size_t nmemb, size_t size)
{
    if (fail_alloc) {
        if (fail_after <= 0)
            return NULL;
        fail_after--;
    }
    if (size && nmemb > SIZE_MAX / size)
        return NULL;
    void *p = calloc(nmemb, size);
    if (p)
        allocs++;
    return p;
}

void test_free(void *ptr)
{
    if (ptr)
        allocs--;
    free(ptr);
}

static void test_simple_fold(void)
{
    ir_builder_t ir;
    ir_builder_init(&ir);
    ir_value_t a = ir_build_const(&ir, 2);
    ir_value_t b = ir_build_const(&ir, 3);
    ir_value_t add = ir_build_binop(&ir, IR_ADD, a, b);
    (void)add;
    fold_constants(&ir);

    ir_instr_t *i = ir.head;
    ASSERT(i && i->op == IR_CONST && i->imm == 2); i = i->next;
    ASSERT(i && i->op == IR_CONST && i->imm == 3); i = i->next;
    ASSERT(i && i->op == IR_CONST && i->imm == 5 && i->next == NULL);

    ir_builder_free(&ir);
    ASSERT(allocs == 0);
}

static void test_chain_fold(void)
{
    ir_builder_t ir;
    ir_builder_init(&ir);
    ir_value_t c1 = ir_build_const(&ir, 1);
    ir_value_t c2 = ir_build_const(&ir, 2);
    ir_value_t add1 = ir_build_binop(&ir, IR_ADD, c1, c2);
    ir_value_t c3 = ir_build_const(&ir, 3);
    ir_value_t c4 = ir_build_const(&ir, 4);
    ir_value_t add2 = ir_build_binop(&ir, IR_ADD, c3, c4);
    ir_value_t mul = ir_build_binop(&ir, IR_MUL, add1, add2);
    (void)mul;
    fold_constants(&ir);

    ir_instr_t *i = ir.head;
    ASSERT(i && i->op == IR_CONST && i->imm == 1); i = i->next;
    ASSERT(i && i->op == IR_CONST && i->imm == 2); i = i->next;
    ASSERT(i && i->op == IR_CONST && i->imm == 3); i = i->next;
    ASSERT(i && i->op == IR_CONST && i->imm == 4); i = i->next;
    ASSERT(i && i->op == IR_CONST && i->imm == 7); i = i->next;
    ASSERT(i && i->op == IR_CONST && i->imm == 21 && i->next == NULL);

    ir_builder_free(&ir);
    ASSERT(allocs == 0);
}

static void test_large_values(void)
{
    ir_builder_t ir;
    ir_builder_init(&ir);
    ir_value_t c1 = ir_build_const(&ir, 123456789);
    ir_value_t c2 = ir_build_const(&ir, 987654321);
    ir_build_binop(&ir, IR_ADD, c1, c2);
    fold_constants(&ir);

    ir_instr_t *i = ir.head; i = i->next; i = i->next; /* skip first two consts */
    ASSERT(i && i->op == IR_CONST && i->imm == 1111111110 && i->next == NULL);

    ir_builder_free(&ir);
    ASSERT(allocs == 0);
}

static void test_alloc_fail(void)
{
    ir_builder_t ir;
    ir_builder_init(&ir);
    ir_value_t a = ir_build_const(&ir, 1);
    ir_value_t b = ir_build_const(&ir, 2);
    ir_build_binop(&ir, IR_ADD, a, b);

    fail_alloc = 1; fail_after = 1;
    fold_constants(&ir);
    fail_alloc = 0;

    ir_instr_t *i = ir.head;
    ASSERT(i && i->op == IR_CONST && i->imm == 1); i = i->next;
    ASSERT(i && i->op == IR_CONST && i->imm == 2); i = i->next;
    ASSERT(i && i->op == IR_ADD && i->next == NULL); /* not folded */

    ir_builder_free(&ir);
    ASSERT(allocs == 0);
}

int main(void)
{
    test_simple_fold();
    test_chain_fold();
    test_large_values();
    test_alloc_fail();
    if (failures == 0)
        printf("All opt_fold tests passed\n");
    else
        printf("%d opt_fold test(s) failed\n", failures);
    return failures ? 1 : 0;
}
