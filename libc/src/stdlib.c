#include <stddef.h>
#include "stdlib.h"
#include "../internal/_vc_syscalls.h"

void exit(int status)
{
    _vc_exit(status);
    for (;;)
        ;
}

void *malloc(size_t size)
{
    return _vc_malloc(size);
}

void free(void *ptr)
{
    _vc_free(ptr);
}
