#include <stdio.h>
#include <errno.h>

int fflush(FILE *stream)
{
    (void)stream;
    errno = ENOSPC;
    return EOF;
}
