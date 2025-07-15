#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include "stdio.h"
#include "../internal/_vc_syscalls.h"

int puts(const char *s)
{
    size_t len = strlen(s);
    _vc_write(1, s, len);
    _vc_write(1, "\n", 1);
    return (int)(len + 1);
}

static void flush_buf(const char *buf, size_t *pos, int *total)
{
    if (*pos > 0) {
        _vc_write(1, buf, *pos);
        if (total)
            *total += (int)(*pos);
        *pos = 0;
    }
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
            if (pos == sizeof(out))
                flush_buf(out, &pos, &written);
            continue;
        }

        /* handle format specifier */
        p++;
        if (*p == '%') {
            out[pos++] = '%';
            if (pos == sizeof(out))
                flush_buf(out, &pos, &written);
            continue;
        }

        flush_buf(out, &pos, &written);

        if (*p == 's') {
            const char *s = va_arg(ap, const char *);
            size_t len = strlen(s);
            _vc_write(1, s, len);
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
            _vc_write(1, q, len);
            written += (int)len;
        } else {
            /* unsupported specifier, print it literally */
            out[pos++] = '%';
            if (pos == sizeof(out))
                flush_buf(out, &pos, &written);
            out[pos++] = *p;
            if (pos == sizeof(out))
                flush_buf(out, &pos, &written);
        }
    }

    flush_buf(out, &pos, &written);
    va_end(ap);
    return written;
}
