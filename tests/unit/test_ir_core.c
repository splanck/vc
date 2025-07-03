#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include "ir_core.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

extern void *malloc(size_t size); /* real malloc */
static int fail_malloc = 0;
static int fail_after = 0;
void *test_malloc(size_t size)
{
    if (fail_malloc) {
        if (fail_after <= 0)
            return NULL;
        fail_after--;
    }
    return malloc(size);
}

void *test_calloc(size_t nmemb, size_t size)
{
    if (size && nmemb > SIZE_MAX / size)
        return NULL;
    void *ptr = test_malloc(nmemb * size);
    if (ptr)
        memset(ptr, 0, nmemb * size);
    return ptr;
}

static void test_wstring_alloc_fail(void)
{
    ir_builder_t b;
    ir_builder_init(&b);
    fail_malloc = 1;
    ir_value_t v = ir_build_wstring(&b, "abc");
    ASSERT(v.id == 0);
    ASSERT(b.head == NULL && b.tail == NULL);
    fail_malloc = 0;
    ir_builder_free(&b);
}

static void test_many_ids(void)
{
    ir_builder_t b;
    ir_builder_init(&b);
    const int count = 10000;
    for (int i = 0; i < count; i++) {
        ir_value_t v = ir_build_const(&b, i);
        ASSERT(v.id == i + 1);
    }
    ASSERT(b.next_value_id == (size_t)count + 1);
    ir_builder_free(&b);
}

static void test_id_overflow(void)
{
    ir_builder_t b;
    ir_builder_init(&b);
    b.next_value_id = (size_t)INT_MAX - 1;
    ir_value_t v = ir_build_const(&b, 0);
    ASSERT(v.id == INT_MAX - 1);
    ASSERT(b.next_value_id == (size_t)INT_MAX);

    pid_t pid = fork();
    ASSERT(pid >= 0);
    if (pid == 0) {
        b.next_value_id = (size_t)INT_MAX;
        ir_build_const(&b, 0);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ASSERT(WIFEXITED(status) && WEXITSTATUS(status) != 0);
    ir_builder_free(&b);
}

static void test_strdup_fail_load(void)
{
    ir_builder_t b;
    ir_builder_init(&b);
    fail_malloc = 1;
    fail_after = 1; /* fail on vc_strdup */
    ir_value_t v = ir_build_load(&b, "x");
    ASSERT(v.id == 0);
    ASSERT(b.head == NULL && b.tail == NULL);
    fail_malloc = 0;
    ir_builder_free(&b);
}

static void test_strdup_fail_string(void)
{
    ir_builder_t b;
    ir_builder_init(&b);
    fail_malloc = 1;
    fail_after = 1; /* fail duplicating label */
    ir_value_t v = ir_build_string(&b, "abc");
    ASSERT(v.id == 0);
    ASSERT(b.head == NULL && b.tail == NULL);
    fail_after = 2; /* fail duplicating data */
    v = ir_build_string(&b, "abc");
    ASSERT(v.id == 0);
    ASSERT(b.head == NULL && b.tail == NULL);
    fail_malloc = 0;
    ir_builder_free(&b);
}

static void test_strdup_fail_wstring(void)
{
    ir_builder_t b;
    ir_builder_init(&b);
    fail_malloc = 1;
    fail_after = 2; /* fail duplicating label after buffer alloc */
    ir_value_t v = ir_build_wstring(&b, "abc");
    ASSERT(v.id == 0);
    ASSERT(b.head == NULL && b.tail == NULL);
    fail_malloc = 0;
    ir_builder_free(&b);
}

static void test_return_agg_opcode(void)
{
    ir_builder_t b;
    ir_builder_init(&b);
    ir_value_t p = ir_build_const(&b, 0);
    ir_build_return_agg(&b, p);
    ir_instr_t *i = b.head;
    ASSERT(i && i->op == IR_CONST); i = i->next;
    ASSERT(i && i->op == IR_RETURN_AGG && i->src1 == p.id);
    ir_builder_free(&b);
}

int main(void)
{
    test_wstring_alloc_fail();
    test_many_ids();
    test_id_overflow();
    test_strdup_fail_load();
    test_strdup_fail_string();
    test_strdup_fail_wstring();
    test_return_agg_opcode();
    if (failures == 0)
        printf("All ir_core tests passed\n");
    else
        printf("%d ir_core test(s) failed\n", failures);
    return failures ? 1 : 0;
}
