#include "stdio.h"
#include "../internal/_vc_syscalls.h"

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
            stream->eof = 1;
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

