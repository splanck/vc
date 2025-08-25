#include <fcntl.h>
#include "errno.h"
#include "stdio.h"
#include "stdlib.h"
#include "../internal/_vc_syscalls.h"

FILE *tmpfile(void)
{
#ifdef O_TMPFILE
    long fd = _vc_open(".", O_TMPFILE | O_RDWR, 0600);
    if (fd < 0) {
        return NULL;
    }
    FILE *f = malloc(sizeof(FILE));
    if (!f) {
        _vc_close(fd);
        return NULL;
    }
    f->fd = (int)fd;
    f->err = 0;
    return f;
#else
    errno = ENOSYS;
    return NULL;
#endif
}

