#include "pthread.h"
#include "../internal/_vc_syscalls.h"

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
    _vc_exit(1);
    for (;;)
        ;
}

