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

typedef struct block {
    unsigned long size;
    struct block *next;
} block_t;

static unsigned long cur_brk = 0;
static block_t *free_list = 0;

void *_vc_malloc(unsigned long size)
{
    long ret;
    block_t **prevp = &free_list;
    block_t *curr = free_list;
    while (curr) {
        if (curr->size >= size) {
            *prevp = curr->next;
            return (void *)(curr + 1);
        }
        prevp = &curr->next;
        curr = curr->next;
    }

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
    if (cur_brk > ULONG_MAX - sizeof(block_t) - size)
        return 0;
    unsigned long prev = cur_brk;
    unsigned long new_brk = cur_brk + sizeof(block_t) + size;
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
    block_t *blk = (block_t *)prev;
    blk->size = size;
    cur_brk = new_brk;
    return (void *)(blk + 1);
}

void _vc_free(void *ptr)
{
    if (!ptr)
        return;

    block_t *blk = (block_t *)ptr - 1;
    block_t **prevp = &free_list;
    block_t *curr = free_list;
    while (curr && (unsigned long)curr < (unsigned long)blk) {
        prevp = &curr->next;
        curr = curr->next;
    }

    blk->next = curr;
    if (curr && (unsigned long)blk + sizeof(block_t) + blk->size == (unsigned long)curr) {
        blk->size += sizeof(block_t) + curr->size;
        blk->next = curr->next;
    }

    if (*prevp && (unsigned long)*prevp + sizeof(block_t) + (*prevp)->size == (unsigned long)blk) {
        (*prevp)->size += sizeof(block_t) + blk->size;
        (*prevp)->next = blk->next;
    } else {
        *prevp = blk;
    }
}
