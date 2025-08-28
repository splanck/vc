#include <stdio.h>
#include <errno.h>

int main(void)
{
    FILE *f = fopen("input.txt", "r");
    if (!f)
        return 1;
    char buf[64];
    if (fgets(buf, sizeof(buf), f))
        printf("%s", buf);

    errno = 0;
    if (fprintf(f, "fail") >= 0 || !f->err || errno == 0) {
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}
