/*
 * Low level syscall helpers for vc's internal libc.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC__VC_SYSCALLS_H
#define VC__VC_SYSCALLS_H

#ifdef __x86_64__
#define VC_SYS_WRITE 1
#define VC_SYS_EXIT 60
#define VC_SYS_BRK 12
#elif defined(__i386__)
#define VC_SYS_WRITE 4
#define VC_SYS_EXIT 1
#define VC_SYS_BRK 45
#endif


#endif /* VC__VC_SYSCALLS_H */
