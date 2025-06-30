#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

typedef struct { const char *name; int op; } inline_func_t;

static int try_grow(inline_func_t **out, size_t *cap, size_t count)
{
    size_t max_cap = SIZE_MAX / sizeof(**out);
    if (count == *cap) {
        size_t new_cap;
        if (*cap) {
            if (*cap > max_cap / 2) {
                fprintf(stderr, "too many inline functions\n");
                return 0;
            }
            new_cap = *cap * 2;
        } else {
            new_cap = 4;
        }
        if (new_cap > max_cap)
            new_cap = max_cap;
        inline_func_t *tmp = realloc(*out, new_cap * sizeof(**out));
        if (!tmp)
            return 0;
        *out = tmp;
        *cap = new_cap;
    }
    return 1;
}

int main(void)
{
    size_t cap = SIZE_MAX / sizeof(inline_func_t);
    inline_func_t *vec = malloc(cap * sizeof(inline_func_t));
    size_t count = cap;
    if (!try_grow(&vec, &cap, count))
        return 0;
    return 1;
}
