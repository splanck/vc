#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include "stdio.h"
#include "../internal/_vc_syscalls.h"

int puts(const char *s)
{
    size_t len = strlen(s);
    long ret = _vc_write(1, s, len);
    if (ret < (long)len) {
        perror("write");
        return -1;
    }
    ret = _vc_write(1, "\n", 1);
    if (ret < 1) {
        perror("write");
        return -1;
    }
    if (len + 1 > (size_t)INT_MAX)
        return INT_MAX;
    return (int)(len + 1);
}

static int flush_buf(const char *buf, size_t *pos, int *total)
{
    if (*pos > 0) {
        long ret = _vc_write(1, buf, *pos);
        if (ret < (long)(*pos)) {
            perror("write");
            return -1;
        }
        if (total)
            *total += (int)(*pos);
        *pos = 0;
    }
    return 0;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    char out[64];
    size_t pos = 0;
    int written = 0;

    va_start(ap, fmt);
    for (const char *p = fmt; *p; ++p) {
        if (*p != '%') {
            out[pos++] = *p;
            if (pos == sizeof(out)) {
                if (flush_buf(out, &pos, &written) < 0) {
                    va_end(ap);
                    return -1;
                }
            }
            continue;
        }

        /* handle format specifier */
        p++;
        if (*p == '%') {
            out[pos++] = '%';
            if (pos == sizeof(out)) {
                if (flush_buf(out, &pos, &written) < 0) {
                    va_end(ap);
                    return -1;
                }
            }
            continue;
        }

        if (flush_buf(out, &pos, &written) < 0) {
            va_end(ap);
            return -1;
        }

        if (*p == 's') {
            const char *s = va_arg(ap, const char *);
            size_t len = strlen(s);
            long ret = _vc_write(1, s, len);
            if (ret < (long)len) {
                perror("write");
                va_end(ap);
                return -1;
            }
            written += (int)len;
        } else if (*p == 'd') {
            int n = va_arg(ap, int);
            char num[32];
            char *q = num + sizeof(num);
            unsigned int u = (n < 0) ? (unsigned int)(-n) : (unsigned int)n;
            if (n == 0) {
                *--q = '0';
            } else {
                while (u) {
                    *--q = (char)('0' + (u % 10));
                    u /= 10;
                }
                if (n < 0)
                    *--q = '-';
            }
            size_t len = num + sizeof(num) - q;
            long ret2 = _vc_write(1, q, len);
            if (ret2 < (long)len) {
                perror("write");
                va_end(ap);
                return -1;
            }
            written += (int)len;
        } else {
            /* unsupported specifier, print it literally */
            out[pos++] = '%';
            if (pos == sizeof(out)) {
                if (flush_buf(out, &pos, &written) < 0) {
                    va_end(ap);
                    return -1;
                }
            }
            out[pos++] = *p;
            if (pos == sizeof(out)) {
                if (flush_buf(out, &pos, &written) < 0) {
                    va_end(ap);
                    return -1;
                }
            }
        }
    }

    if (flush_buf(out, &pos, &written) < 0) {
        va_end(ap);
        return -1;
    }
    va_end(ap);
    return written;
}
