#include <stdio.h>
#include <limits.h>
int main(void)
{
    int ret = puts("a");
    return (ret == INT_MAX) ? 0 : 1;
}
