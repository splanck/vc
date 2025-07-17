#include <limits.h>
#include <stddef.h>

size_t strlen(const char *s)
{
    (void)s;
    return (size_t)INT_MAX + 10;
}

long _vc_write(int fd, const void *buf, unsigned long count)
{
    (void)fd; (void)buf;
    return (long)count;
}
