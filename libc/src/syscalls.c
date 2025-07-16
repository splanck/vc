#include "../internal/_vc_syscalls.h"

#ifdef __x86_64__
#define SYSCALL_INVOKE(num, a1, a2, a3) \
    long ret; \
    __asm__ volatile ("syscall" \
                      : "=a"(ret) \
                      : "a"(num), "D"(a1), "S"(a2), "d"(a3) \
                      : "rcx", "r11", "memory"); \
    return ret
#elif defined(__i386__)
#define SYSCALL_INVOKE(num, a1, a2, a3) \
    long ret; \
    __asm__ volatile ("int $0x80" \
                      : "=a"(ret) \
                      : "a"(num), "b"(a1), "c"(a2), "d"(a3) \
                      : "memory"); \
    return ret
#endif

long _vc_write(int fd, const void *buf, unsigned long count)
{
    SYSCALL_INVOKE(VC_SYS_WRITE, fd, buf, count);
}

long _vc_read(int fd, void *buf, unsigned long count)
{
    SYSCALL_INVOKE(VC_SYS_READ, fd, buf, count);
}

long _vc_open(const char *path, int flags, int mode)
{
    SYSCALL_INVOKE(VC_SYS_OPEN, path, flags, mode);
}

long _vc_close(int fd)
{
    SYSCALL_INVOKE(VC_SYS_CLOSE, fd, 0, 0);
}

__attribute__((noreturn, naked)) void _vc_exit(int status)
{
    (void)status;
#ifdef __x86_64__
    __asm__ volatile(
        "mov $60, %rax\n"
        "syscall\n"
        "hlt\n"
    );
#elif defined(__i386__)
    __asm__ volatile(
        "mov $1, %eax\n"
        "int $0x80\n"
        "hlt\n"
    );
#endif
}

void *_vc_malloc(unsigned long size)
{
    static unsigned long cur_brk = 0;
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
    unsigned long prev = cur_brk;
    unsigned long new_brk = cur_brk + size;
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
    cur_brk = new_brk;
    return (void *)prev;
}

void _vc_free(void *ptr)
{
    (void)ptr;
}
