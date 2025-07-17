#include <stdio.h>
#include "util.h"

int main(void)
{
    size_t val = 0;
    if (vc_strtoul_size("-1", &val)) {
        printf("negative accepted\n");
        return 1;
    }
    if (!vc_strtoul_size("123", &val) || val != (size_t)123u) {
        printf("parse failed\n");
        return 1;
    }
    printf("All vc_strtoul_size tests passed\n");
    return 0;
}
