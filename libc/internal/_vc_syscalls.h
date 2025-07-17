/*
 * Low level syscall helpers for vc's internal libc.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC__VC_SYSCALLS_H
#define VC__VC_SYSCALLS_H

#include <stddef.h>

#ifdef __x86_64__
#define VC_SYS_WRITE 1
#define VC_SYS_READ 0
#define VC_SYS_OPEN 2
#define VC_SYS_CLOSE 3
#define VC_SYS_EXIT 60
#define VC_SYS_BRK 12
#elif defined(__i386__)
#define VC_SYS_WRITE 4
#define VC_SYS_READ 3
#define VC_SYS_OPEN 5
#define VC_SYS_CLOSE 6
#define VC_SYS_EXIT 1
#define VC_SYS_BRK 45
#endif


long _vc_write(int, const void *, unsigned long);
long _vc_read(int, void *, unsigned long);
long _vc_open(const char *, int, int);
long _vc_close(int);
void _vc_exit(int) __attribute__((noreturn));
void *_vc_malloc(unsigned long);
void _vc_free(void *);


#endif /* VC__VC_SYSCALLS_H */
