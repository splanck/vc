#include <stddef.h>
#include "stdlib.h"
#include "../internal/_vc_syscalls.h"
void _exit(int) __attribute__((noreturn));

void exit(int status) __attribute__((noreturn));
void exit(int status)
{
    void (*vc_exit_ptr)(int) = _vc_exit;
    vc_exit_ptr(status);
    const char msg[] = "vc libc: exit syscall failed\n";
    _vc_write(2, msg, sizeof(msg) - 1);
    vc_exit_ptr(1);
    _exit(1);
}

void *malloc(size_t size)
{
    return _vc_malloc(size);
}

void free(void *ptr)
{
    _vc_free(ptr);
}
