#include <stddef.h>
#include "stdlib.h"
#include "../internal/_vc_syscalls.h"
void _exit(int);

void exit(int status)
{
    long ret = _vc_exit(status);
    if (ret < 0) {
        const char msg[] = "vc libc: exit syscall failed\n";
        _vc_write(2, msg, sizeof(msg) - 1);
        ret = _vc_exit(1);
        if (ret < 0)
            _exit(1);
    }
}

void *malloc(size_t size)
{
    return _vc_malloc(size);
}

void free(void *ptr)
{
    _vc_free(ptr);
}
