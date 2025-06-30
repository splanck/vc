/*
 * Growable vector implementation.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "vector.h"

/*
 * Prepare a vector to hold elements of "elem_size" bytes.  The vector
 * initially contains zero elements and no allocated storage.  The
 * element size must be non-zero.
 */
void vector_init(vector_t *vec, size_t elem_size)
{
    if (!vec)
        return;
    /* element size is documented as non-zero */
    assert(elem_size > 0);
    vec->data = NULL;
    vec->count = 0;
    vec->cap = 0;
    vec->elem_size = elem_size;
}

/*
 * Append one element to the end of the vector.  The element data is
 * copied from the memory pointed to by "elem".  The vector grows as
 * needed.  Returns 1 on success and 0 on invalid arguments.  The push
 * fails if the vector element size is zero.
*/
int vector_push(vector_t *vec, const void *elem)
{
    if (!vec || !elem || vec->elem_size == 0)
        return 0;
    if (vec->count >= vec->cap) {
        size_t new_cap;
        if (vec->cap) {
            if (vec->cap > SIZE_MAX / 2)
                return 0;
            new_cap = vec->cap * 2;
        } else {
            new_cap = 16;
        }
        if (new_cap > SIZE_MAX / vec->elem_size)
            return 0;
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

/*
 * Release all memory owned by the vector and reset its fields.  The
 * vector may be reused after calling vector_init() again.
 */
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
