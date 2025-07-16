#include <errno.h>
long _vc_write(int fd, const void *buf, unsigned long count) {
    (void)fd; (void)buf;
    errno = ENOSPC;
    return (count > 0) ? (long)(count - 1) : 0;
}
