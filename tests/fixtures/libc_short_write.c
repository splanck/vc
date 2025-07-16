#include <stdio.h>
int main(void) {
    if (puts("hello") < 0)
        return 1;
    return 0;
}
