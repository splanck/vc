#include "../internal/_vc_syscalls.h"

void _exit(int status) __attribute__((noreturn));

void _exit(int status)
{
#ifdef __x86_64__
    __asm__ volatile ("syscall"
                      :
                      : "a"(VC_SYS_EXIT), "D"(status)
                      : "rcx", "r11", "memory");
#elif defined(__i386__)
    __asm__ volatile ("int $0x80"
                      :
                      : "a"(VC_SYS_EXIT), "b"(status)
                      : "memory");
#endif
    __builtin_unreachable();
}
