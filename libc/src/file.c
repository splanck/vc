#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "stdio.h"
#include "stdlib.h"
#include "../internal/_vc_syscalls.h"

FILE *fopen(const char *path, const char *mode)
{
    int flags = 0;
    int perm = 0;
    if (mode && mode[0] == 'r') {
        flags = 0; /* O_RDONLY */
    } else if (mode && mode[0] == 'w') {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        perm = 0666;
    } else {
        return NULL;
    }

    long fd = _vc_open(path, flags, perm);
    if (fd < 0)
        return NULL;

    FILE *f = malloc(sizeof(FILE));
    if (!f) {
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

static int flush_buf_fd(int fd, const char *buf, size_t *pos, int *total)
{
    if (*pos > 0) {
        long ret = _vc_write(fd, buf, *pos);
        if (ret < (long)(*pos))
            return -1;
        if (total)
            *total += (int)(*pos);
        *pos = 0;
    }
    return 0;
}

int fprintf(FILE *stream, const char *fmt, ...)
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
            width = width * 10 + (*p - '0');
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
            }

            long ret = _vc_write(stream->fd, s, len);
            if (ret < (long)len) {
                va_end(ap);
                return -1;
            }
            written += (int)len;
        } else {
            long ret = _vc_write(stream->fd, "%", 1);
            if (ret < 1) {
                va_end(ap);
                return -1;
            }
            written += 1;
            if (width) {
                const char *q = start;
                size_t l = (size_t)(p - start);
                ret = _vc_write(stream->fd, q, l);
                if (ret < (long)l) {
                    va_end(ap);
                    return -1;
                }
                written += (int)l;
            }
            ret = _vc_write(stream->fd, p, 1);
            if (ret < 1) {
                va_end(ap);
                return -1;
            }
            written += 1;
        }
    }

    if (flush_buf_fd(stream->fd, out, &pos, &written) < 0) {
        va_end(ap);
        return -1;
    }
    va_end(ap);
    return written;
}

char *fgets(char *s, int size, FILE *stream)
{
    if (size <= 0)
        return NULL;
    int i = 0;
    while (i < size - 1) {
        char c;
        long r = _vc_read(stream->fd, &c, 1);
        if (r < 0) {
            stream->err = 1;
            return NULL;
        }
        if (r == 0) {
            if (i == 0)
                return NULL;
            break;
        }
        s[i++] = c;
        if (c == '\n')
            break;
    }
    s[i] = '\0';
    return s;
}

FILE *tmpfile(void)
{
    errno = ENOSYS;
    return NULL;
}

void perror(const char *msg)
{
    if (msg && *msg) {
        size_t len = strlen(msg);
        _vc_write(2, msg, len);
        _vc_write(2, ": ", 2);
    }
    _vc_write(2, "error\n", 6);
}
