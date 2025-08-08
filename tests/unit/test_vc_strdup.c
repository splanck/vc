#include <stdio.h>
#include "util.h"

int main(void)
{
    if (vc_strdup(NULL) != NULL) {
        printf("vc_strdup NULL check failed\n");
        return 1;
    }
    vector_t v;
    vector_init(&v, sizeof(char *));
    vector_push(&v, vc_strdup(NULL));
    free_string_vector(&v);
    printf("vc_strdup NULL tests passed\n");
    return 0;
}
