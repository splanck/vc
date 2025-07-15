/*
 * Minimal stdarg definitions for vc's internal libc.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_STDARG_H
#define VC_STDARG_H

#define va_list char *
#define va_start(ap, last) (ap = (char *)&(last) + sizeof(last))
#define va_arg(ap, type) (*(type *)((ap += sizeof(type)) - sizeof(type)))
#define va_end(ap)       (ap = (va_list)0)
#define va_copy(dest, src) ((dest) = (src))

#endif /* VC_STDARG_H */
