#include <stdio.h>

int main(void) {
    FILE *f = fopen("tests/fixtures/line_comment.c", "r");
    if (!f)
        return 1;
    char buf[64];
    if (!fgets(buf, sizeof(buf), f))
        return 2;
    if (f->eof || f->err)
        return 3;
    while (fgets(buf, sizeof(buf), f))
        ;
    if (!f->eof || f->err)
        return 4;
    fclose(f);
    return 0;
}
