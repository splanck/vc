/*
 * Minimal stddef definitions for vc's internal libc.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_STDDEF_H
#define VC_STDDEF_H

#ifdef __x86_64__
typedef unsigned long size_t;
typedef long ptrdiff_t;
#else
typedef unsigned int size_t;
typedef int ptrdiff_t;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* VC_STDDEF_H */
