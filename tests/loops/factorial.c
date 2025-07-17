#include <stdio.h>

int main(void) {
    int n = 5;
    int fact = 1;
    int i = 1;
    while (i <= n) {
        fact = fact * i;
        i = i + 1;
    }
    printf("factorial(%d) = %d\n", n, fact);
    return 0;
}
