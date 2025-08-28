#include <stddef.h>
#include <string.h>
#include "errno.h"
#include "stdio.h"
#include "../internal/_vc_syscalls.h"

typedef struct {
    int code;
    const char *name;
} err_entry_t;

static const err_entry_t err_table[] = {
    { ENOENT, "ENOENT" },
    { EINTR,  "EINTR"  },
    { ENOMEM, "ENOMEM" },
    { ENOSYS, "ENOSYS" },
    { ENOSPC, "ENOSPC" },
    { ENAMETOOLONG, "ENAMETOOLONG" }
};

void perror(const char *msg)
{
    int err = *_errno_location();
    char buf[64];
    size_t pos = 0;

    if (msg && *msg) {
        size_t len = strlen(msg);
        if (len > sizeof(buf) - 2)
            len = sizeof(buf) - 2;
        memcpy(buf, msg, len);
        pos = len;
        if (pos < sizeof(buf))
            buf[pos++] = ':';
        if (pos < sizeof(buf))
            buf[pos++] = ' ';
    }

    const char *emsg = NULL;
    for (size_t i = 0; i < sizeof(err_table) / sizeof(err_table[0]); ++i) {
        if (err_table[i].code == err) {
            emsg = err_table[i].name;
            break;
        }
    }

    if (emsg) {
        size_t len = strlen(emsg);
        if (len > sizeof(buf) - pos - 1)
            len = sizeof(buf) - pos - 1;
        memcpy(buf + pos, emsg, len);
        pos += len;
    } else {
        char num[32];
        char *p = num + sizeof(num);
        unsigned int u = (unsigned int)err;
        if (u == 0) {
            *--p = '0';
        } else {
            while (u) {
                *--p = (char)('0' + (u % 10));
                u /= 10;
            }
        }
        size_t nlen = (size_t)(num + sizeof(num) - p);
        if (nlen > sizeof(buf) - pos - 1)
            nlen = sizeof(buf) - pos - 1;
        memcpy(buf + pos, p, nlen);
        pos += nlen;
    }

    if (pos < sizeof(buf))
        buf[pos++] = '\n';

    _vc_write(2, buf, pos);
}

