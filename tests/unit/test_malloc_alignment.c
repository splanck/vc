#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

struct strict_align {
    char c;
} __attribute__((aligned(sizeof(void *))));

int main(void)
{
    enum { COUNT = 10000 };
    struct strict_align *ptrs[COUNT];

    for (int i = 0; i < COUNT; ++i) {
        ptrs[i] = malloc(sizeof(struct strict_align));
        if (!ptrs[i] || ((uintptr_t)ptrs[i] % sizeof(void *)) != 0) {
            printf("alignment failed on initial allocation %d\n", i);
            return 1;
        }
    }

    for (int i = 0; i < COUNT; i += 2)
        free(ptrs[i]);

    for (int i = 0; i < COUNT; i += 2) {
        ptrs[i] = malloc(sizeof(struct strict_align));
        if (!ptrs[i] || ((uintptr_t)ptrs[i] % sizeof(void *)) != 0) {
            printf("alignment failed on re-allocation %d\n", i);
            return 1;
        }
        free(ptrs[i]);
    }

    for (int i = 1; i < COUNT; i += 2)
        free(ptrs[i]);

    printf("malloc alignment stress passed\n");
    return 0;
}

