/*
 * Minimal errno declarations for vc's internal libc.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_ERRNO_H
#define VC_ERRNO_H

/* Global errno variable accessible through _errno_location(). */
extern int errno;

/* Return address of thread-local errno. In this libc it is a single
 * static variable, so the function simply returns its address.
 */
int *_errno_location(void);

/* Map errno to the function above. */
#define errno (*_errno_location())

/* Minimal set of error codes used by the library and tests. */
#define ENOENT        2
#define EINTR         4
#define ENOMEM       12
#define ENAMETOOLONG 36
#define ENOSYS       38
#define ENOSPC       28

#endif /* VC_ERRNO_H */
