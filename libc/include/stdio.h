/*
 * Minimal stdio declarations for vc's internal libc.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_STDIO_H
#define VC_STDIO_H

int puts(const char *);
int printf(const char *, ...);

#endif /* VC_STDIO_H */
