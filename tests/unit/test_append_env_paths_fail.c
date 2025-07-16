#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preproc_path.h"
#include "util.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

extern int vector_push(vector_t *vec, const void *elem); /* real impl */
static int fail_at = 0;
static int call_count = 0;
int test_vector_push(vector_t *vec, const void *elem)
{
    call_count++;
    if (fail_at && call_count == fail_at)
        return 0;
    return vector_push(vec, elem);
}

extern void *malloc(size_t size); /* real malloc */
extern void *calloc(size_t nmemb, size_t size); /* real calloc */
extern void *realloc(void *ptr, size_t size); /* real realloc */
extern void free(void *ptr); /* real free */
static int allocs = 0;

void *test_malloc(size_t size)
{
    void *p = malloc(size);
    if (p)
        allocs++;
    return p;
}

void *test_calloc(size_t nmemb, size_t size)
{
    if (size && nmemb > SIZE_MAX / size)
        return NULL;
    void *p = calloc(nmemb, size);
    if (p)
        allocs++;
    return p;
}

void *test_realloc(void *ptr, size_t size)
{
    if (!ptr)
        return test_malloc(size);
    void *p = realloc(ptr, size);
    if (p && p != ptr) {
        allocs++;
        allocs--; /* previous ptr freed */
    }
    return p;
}

void test_free(void *ptr)
{
    if (ptr)
        allocs--;
    free(ptr);
}

static void test_fail_second(void)
{
    vector_t dirs; vector_init(&dirs, sizeof(char *));
    fail_at = 2; call_count = 0;
    ASSERT(!append_env_paths("foo:bar", &dirs));
    ASSERT(dirs.count == 0 && dirs.cap == 0);
    ASSERT(allocs == 0);
    vector_free(&dirs);
}

int main(void)
{
    test_fail_second();
    if (failures == 0)
        printf("All append_env_paths_fail tests passed\n");
    else
        printf("%d append_env_paths_fail test(s) failed\n", failures);
    return failures ? 1 : 0;
}
