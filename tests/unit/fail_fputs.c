#include <stdio.h>
#include <errno.h>

int fputs(const char *s, FILE *stream)
{
    (void)s; (void)stream;
    errno = ENOSPC;
    return EOF;
}
