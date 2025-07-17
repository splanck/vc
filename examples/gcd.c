#include <stdio.h>

int gcd(int a, int b)
{
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int main(void)
{
    int x = 36, y = 24;
    printf("gcd(%d, %d) = %d\n", x, y, gcd(x, y));
    return 0;
}
