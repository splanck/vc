#include <stddef.h>
#include <string.h>
#include "stdio.h"
#include "../internal/_vc_syscalls.h"

int puts(const char *s)
{
    size_t len = strlen(s);
    _vc_write(1, s, len);
    _vc_write(1, "\n", 1);
    return (int)(len + 1);
}

int printf(const char *fmt, ...)
{
    size_t len = strlen(fmt);
    _vc_write(1, fmt, len);
    return (int)len;
}
