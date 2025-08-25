#include "pthread.h"
#include "../internal/_vc_syscalls.h"
void _exit(int) __attribute__((noreturn));

/*
 * Stub implementation for pthread_create in the single-threaded libc.
 * Prints an error message and exits; this function never returns.
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg)
{
    (void)thread;
    (void)attr;
    (void)start_routine;
    (void)arg;
    const char msg[] =
        "vc libc is single-threaded; pthread_create unsupported\n";
    _vc_write(2, msg, sizeof(msg) - 1);
    long (*exit_ptr)(int) = _vc_exit;
    long ret = exit_ptr(1);
    if (ret < 0) {
        const char fail[] = "vc libc: exit syscall failed\n";
        _vc_write(2, fail, sizeof(fail) - 1);
        _exit(1);
    }
    return -1;
}

