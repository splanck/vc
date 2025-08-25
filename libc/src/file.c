#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include "errno.h"
#include "stdio.h"
#include <limits.h>
#include "stdlib.h"
#include "../internal/_vc_syscalls.h"

FILE *fopen(const char *path, const char *mode)
{
    if (!mode || !mode[0])
        return NULL;

    int flags = 0;
    int perm = 0;
    int plus = 0;

    for (const char *p = mode + 1; *p; ++p) {
        if (*p == '+') {
            plus = 1;
        } else if (*p == 'b') {
            /* ignore */
        } else {
            return NULL;
        }
    }

    if (mode[0] == 'r') {
        flags = plus ? O_RDWR : O_RDONLY;
    } else if (mode[0] == 'w') {
        flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
        perm = 0666;
    } else if (mode[0] == 'a') {
        flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
        perm = 0666;
    } else {
        return NULL;
    }

    long fd = _vc_open(path, flags, perm);
    if (fd < 0) {
        errno = -fd;
        return NULL;
    }

    FILE *f = malloc(sizeof(FILE));
    if (!f) {
        errno = ENOMEM;
        _vc_close(fd);
        return NULL;
    }
    f->fd = (int)fd;
    f->err = 0;
    return f;
}

int fclose(FILE *stream)
{
    if (!stream)
        return -1;
    int ret = (int)_vc_close(stream->fd);
    free(stream);
    return ret;
}

static int flush_buf_fd(int fd, const char *buf, size_t *pos, long *total)
{
    if (*pos > 0) {
        size_t remaining = *pos;
        const char *p = buf;
        while (remaining > 0) {
            long ret = _vc_write(fd, p, remaining);
            if (ret < 0) {
                if (errno == EINTR)
                    continue;
                return -1;
            }
            p += (size_t)ret;
            remaining -= (size_t)ret;
        }
        if (total) {
            *total += (long)(*pos);
            if (*total > INT_MAX)
                *total = INT_MAX;
        }
        *pos = 0;
    }
    return 0;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    char out[64];
    size_t pos = 0;
    long written = 0;

    va_start(ap, fmt);
    for (const char *p = fmt; *p; ++p) {
        if (*p != '%') {
            out[pos++] = *p;
            if (pos == sizeof(out))
                if (flush_buf_fd(stream->fd, out, &pos, &written) < 0) {
                    va_end(ap);
                    return -1;
                }
            continue;
        }

        p++;
        if (*p == '%') {
            out[pos++] = '%';
            if (pos == sizeof(out))
                if (flush_buf_fd(stream->fd, out, &pos, &written) < 0) {
                    va_end(ap);
                    return -1;
                }
            continue;
        }

        if (flush_buf_fd(stream->fd, out, &pos, &written) < 0) {
            va_end(ap);
            return -1;
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
                if (_vc_write(stream->fd, " ", 1) < 1) {
                    va_end(ap);
                    return -1;
                }
                written += 1;
                if (written > INT_MAX)
                    written = INT_MAX;
            }

            long ret = _vc_write(stream->fd, s, len);
            if (ret < (long)len) {
                va_end(ap);
                return -1;
            }
            written += (long)len;
            if (written > INT_MAX)
                written = INT_MAX;
        } else {
            long ret = _vc_write(stream->fd, "%", 1);
            if (ret < 1) {
                va_end(ap);
                return -1;
            }
            written += 1;
            if (written > INT_MAX)
                written = INT_MAX;
            if (width) {
                const char *q = start;
                size_t l = (size_t)(p - start);
                ret = _vc_write(stream->fd, q, l);
                if (ret < (long)l) {
                    va_end(ap);
                    return -1;
                }
                written += (long)l;
                if (written > INT_MAX)
                    written = INT_MAX;
            }
            ret = _vc_write(stream->fd, p, 1);
            if (ret < 1) {
                va_end(ap);
                return -1;
            }
            written += 1;
            if (written > INT_MAX)
                written = INT_MAX;
        }
    }

    if (flush_buf_fd(stream->fd, out, &pos, &written) < 0) {
        va_end(ap);
        return -1;
    }
    va_end(ap);
    if (written > INT_MAX)
        return INT_MAX;
    return (int)written;
}
