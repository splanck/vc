#include <stdio.h>

int main(void)
{
    FILE *f = fopen("example.txt", "w");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fprintf(f, "Hello, file!\n");
    fclose(f);

    f = fopen("example.txt", "r");
    if (!f) {
        perror("fopen");
        return 1;
    }

    char buf[64];
    if (fgets(buf, sizeof(buf), f))
        printf("Read: %s", buf);

    fclose(f);
    return 0;
}
