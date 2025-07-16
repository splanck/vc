#include <errno.h>
long _vc_write(int fd, const void *buf, unsigned long count) {
    (void)fd; (void)buf; (void)count;
    errno = ENOSPC;
    return -1;
}
