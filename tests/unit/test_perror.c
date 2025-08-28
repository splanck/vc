#include <stdio.h>
#include "errno.h"
#include "../../libc/internal/_vc_syscalls.h"

int main(void) {
    long fd = _vc_open("tests/unit/no_such_file", 0, 0);
    if (fd >= 0) {
        _vc_close((int)fd);
        return 1;
    }
    perror("open");
    return 0;
}
