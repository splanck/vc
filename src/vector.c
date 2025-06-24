#include <stdlib.h>
#include <string.h>
#include "vector.h"

void vector_init(vector_t *vec, size_t elem_size)
{
    if (!vec)
        return;
    vec->data = NULL;
    vec->count = 0;
    vec->cap = 0;
    vec->elem_size = elem_size;
}

int vector_push(vector_t *vec, const void *elem)
{
    if (!vec || !elem)
        return 0;
    if (vec->count >= vec->cap) {
        size_t new_cap = vec->cap ? vec->cap * 2 : 16;
        void *tmp = realloc(vec->data, new_cap * vec->elem_size);
        if (!tmp)
            return 0;
        vec->data = tmp;
        vec->cap = new_cap;
    }
    memcpy((char *)vec->data + vec->count * vec->elem_size, elem, vec->elem_size);
    vec->count++;
    return 1;
}

void vector_free(vector_t *vec)
{
    if (!vec)
        return;
    free(vec->data);
    vec->data = NULL;
    vec->count = 0;
    vec->cap = 0;
    vec->elem_size = 0;
}
