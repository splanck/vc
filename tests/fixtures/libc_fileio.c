#include <stdio.h>

int main(void)
{
    FILE *f = fopen("input.txt", "r");
    if (!f)
        return 1;
    char buf[64];
    if (fgets(buf, sizeof(buf), f))
        printf("%s", buf);
    fclose(f);
    return 0;
}
