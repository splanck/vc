#include <stdio.h>

int main(void)
{
    int a = 5;
    int b = 2;

    printf("%d + %d = %d\n", a, b, a + b);
    printf("%d - %d = %d\n", a, b, a - b);
    printf("%d * %d = %d\n", a, b, a * b);
    printf("%d / %d = %d\n", a, b, a / b);

    int sum = 0;
    for (int i = 1; i <= 10; ++i)
        sum += i;
    printf("Sum 1..10 = %d\n", sum);

    return 0;
}
