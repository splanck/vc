#include "stdio.h"
#include "errno.h"
#include "../internal/_vc_syscalls.h"

char *fgets(char *s, int size, FILE *stream)
{
    if (size <= 0)
        return NULL;
    int i = 0;
    while (i < size - 1) {
        char c;
        long r;
        do {
            r = _vc_read(stream->fd, &c, 1);
        } while (r < 0 && errno == EINTR);
        if (r < 0) {
            stream->err = 1;
            stream->eof = 0;
            return NULL;
        }
        if (r == 0) {
            stream->eof = 1;
            stream->err = 0;
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

