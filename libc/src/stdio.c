#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include "errno.h"
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
        size_t remaining = *pos;
        const char *p = buf;
        while (remaining > 0) {
            long ret = _vc_write(1, p, remaining);
            if (ret < 0) {
                if (errno == EINTR)
                    continue;
                perror("write");
                return -1;
            }
            p += (size_t)ret;
            remaining -= (size_t)ret;
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

        int width = 0;
        const char *start = p;
        while (*p >= '0' && *p <= '9') {
            int digit = *p - '0';
            if (width > INT_MAX / 10 ||
                (width == INT_MAX / 10 && digit > INT_MAX % 10)) {
                width = INT_MAX;
                while (*p >= '0' && *p <= '9')
                    p++;
                break;
            }
            width = width * 10 + digit;
            p++;
        }

        if (flush_buf(out, &pos, &written) < 0) {
            va_end(ap);
            return -1;
        }

        if (*p == 's' || *p == 'c' || *p == 'd' || *p == 'u') {
            char num[32];
            const char *s = NULL;
            size_t len = 0;

            if (*p == 's') {
                s = va_arg(ap, const char *);
                len = strlen(s);
            } else if (*p == 'c') {
                num[0] = (char)va_arg(ap, int);
                s = num;
                len = 1;
            } else if (*p == 'd' || *p == 'u') {
                unsigned int u;
                int neg = 0;
                if (*p == 'd') {
                    int n = va_arg(ap, int);
                    if (n < 0) {
                        neg = 1;
                        u = (unsigned int)(-n);
                    } else {
                        u = (unsigned int)n;
                    }
                } else {
                    u = va_arg(ap, unsigned int);
                }

                char *q = num + sizeof(num);
                if (u == 0) {
                    *--q = '0';
                } else {
                    while (u) {
                        *--q = (char)('0' + (u % 10));
                        u /= 10;
                    }
                }
                if (neg)
                    *--q = '-';
                len = num + sizeof(num) - q;
                s = q;
            }

            int pad = width > (int)len ? width - (int)len : 0;
            while (pad-- > 0) {
                if (_vc_write(1, " ", 1) < 1) {
                    perror("write");
                    va_end(ap);
                    return -1;
                }
                written += 1;
            }

            long ret = _vc_write(1, s, len);
            if (ret < (long)len) {
                perror("write");
                va_end(ap);
                return -1;
            }
            written += (int)len;
        } else {
            /* unsupported specifier, print literally */
            long ret = _vc_write(1, "%", 1);
            if (ret < 1) {
                perror("write");
                va_end(ap);
                return -1;
            }
            written += 1;
            if (width) {
                const char *q = start;
                size_t l = (size_t)(p - start);
                ret = _vc_write(1, q, l);
                if (ret < (long)l) {
                    perror("write");
                    va_end(ap);
                    return -1;
                }
                written += (int)l;
            }
            ret = _vc_write(1, p, 1);
            if (ret < 1) {
                perror("write");
                va_end(ap);
                return -1;
            }
            written += 1;
        }
    }

    if (flush_buf(out, &pos, &written) < 0) {
        va_end(ap);
        return -1;
    }
    va_end(ap);
    return written;
}
