#ifndef VC_VECTOR_H
#define VC_VECTOR_H

#include <stddef.h>

/* Generic growable array */
typedef struct {
    void *data;
    size_t count;
    size_t cap;
    size_t elem_size;
} vector_t;

/* Initialize vector for elements of given size */
void vector_init(vector_t *vec, size_t elem_size);

/* Append an element, returns 0 on failure */
int vector_push(vector_t *vec, const void *elem);

/* Free vector data */
void vector_free(vector_t *vec);

#endif /* VC_VECTOR_H */
