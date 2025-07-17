#include "util.h"
#include <stdio.h>

int main(void)
{
    char buf[4];
    vc_snprintf(buf, sizeof(buf), "longstring");
    (void)buf;
    return 0;
}

