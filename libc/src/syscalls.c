#include "../internal/_vc_syscalls.h"
#include <limits.h>
#include "errno.h"

#ifdef __x86_64__
#define SYSCALL_INVOKE(num, a1, a2, a3) \
    __asm__ volatile ("syscall" \
                      : "=a"(ret) \
                      : "a"(num), "D"(a1), "S"(a2), "d"(a3) \
                      : "rcx", "r11", "memory")
#elif defined(__i386__)
#define SYSCALL_INVOKE(num, a1, a2, a3) \
    __asm__ volatile ("int $0x80" \
                      : "=a"(ret) \
                      : "a"(num), "b"(a1), "c"(a2), "d"(a3) \
                      : "memory")
#endif

long _vc_write(int fd, const void *buf, unsigned long count)
{
    long ret;
    SYSCALL_INVOKE(VC_SYS_WRITE, fd, buf, count);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

long _vc_read(int fd, void *buf, unsigned long count)
{
    long ret;
    SYSCALL_INVOKE(VC_SYS_READ, fd, buf, count);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

long _vc_open(const char *path, int flags, int mode)
{
    long ret;
    SYSCALL_INVOKE(VC_SYS_OPEN, path, flags, mode);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

long _vc_close(int fd)
{
    long ret;
    SYSCALL_INVOKE(VC_SYS_CLOSE, fd, 0, 0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

long _vc_exit(int status)
{
    long ret;
    SYSCALL_INVOKE(VC_SYS_EXIT, status, 0, 0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

static unsigned long cur_brk = 0;

void *_vc_malloc(unsigned long size)
{
    long ret;
    if (cur_brk == 0) {
#ifdef __x86_64__
        __asm__ volatile ("syscall"
                          : "=a"(ret)
                          : "a"(VC_SYS_BRK), "D"(0)
                          : "rcx", "r11", "memory");
#elif defined(__i386__)
        __asm__ volatile ("int $0x80"
                          : "=a"(ret)
                          : "a"(VC_SYS_BRK), "b"(0)
                          : "memory");
#endif
        if (ret <= 0)
            return 0;
        cur_brk = (unsigned long)ret;
    }
    if (size > ULONG_MAX - cur_brk - sizeof(unsigned long))
        return 0;
    unsigned long prev = cur_brk;
    unsigned long new_brk = cur_brk + sizeof(unsigned long) + size;
#ifdef __x86_64__
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(VC_SYS_BRK), "D"(new_brk)
                      : "rcx", "r11", "memory");
#elif defined(__i386__)
    __asm__ volatile ("int $0x80"
                      : "=a"(ret)
                      : "a"(VC_SYS_BRK), "b"(new_brk)
                      : "memory");
#endif
    if (ret < 0 || (unsigned long)ret < new_brk) {
        return 0;
    }
    *(unsigned long *)prev = size;
    cur_brk = new_brk;
    return (void *)(prev + sizeof(unsigned long));
}

void _vc_free(void *ptr)
{
    if (!ptr || cur_brk == 0)
        return;

    unsigned long start = (unsigned long)ptr - sizeof(unsigned long);
    unsigned long size = *(unsigned long *)start;
    unsigned long end = start + sizeof(unsigned long) + size;

    if (end != cur_brk)
        return;

    long ret;
#ifdef __x86_64__
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(VC_SYS_BRK), "D"(start)
                      : "rcx", "r11", "memory");
#elif defined(__i386__)
    __asm__ volatile ("int $0x80"
                      : "=a"(ret)
                      : "a"(VC_SYS_BRK), "b"(start)
                      : "memory");
#endif
    if (ret >= 0 && (unsigned long)ret >= start)
        cur_brk = start;
}
