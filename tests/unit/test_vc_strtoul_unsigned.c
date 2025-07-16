#include <stdio.h>
#include "util.h"

int main(void)
{
    unsigned val = 0;
    if (vc_strtoul_unsigned("-1", &val)) {
        printf("negative accepted\n");
        return 1;
    }
    if (!vc_strtoul_unsigned("123", &val) || val != 123u) {
        printf("parse failed\n");
        return 1;
    }
    printf("All vc_strtoul_unsigned tests passed\n");
    return 0;
}
