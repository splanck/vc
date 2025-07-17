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

        if (*p == 's') {
            const char *s = va_arg(ap, const char *);
            size_t len = strlen(s);
            long ret = _vc_write(stream->fd, s, len);
            if (ret < 0) {
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
            long ret2 = _vc_write(stream->fd, q, len);
            if (ret2 < 0) {
                va_end(ap);
                return -1;
            }
            written += (int)len;
        } else {
            out[pos++] = '%';
            if (pos == sizeof(out))
                if (flush_buf_fd(stream->fd, out, &pos, &written) < 0) {
                    va_end(ap);
                    return -1;
                }
            out[pos++] = *p;
            if (pos == sizeof(out))
                if (flush_buf_fd(stream->fd, out, &pos, &written) < 0) {
                    va_end(ap);
                    return -1;
                }
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
