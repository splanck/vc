#include <stdio.h>

int main(void) {
    FILE *f = fopen("example.txt", "r");
    if (!f) {
        perror("fopen");
        return 1;
    }
    char buf[64];
    int lines = 0;
    while (!f->eof) {
        if (!fgets(buf, sizeof(buf), f)) {
            if (f->err) {
                perror("fgets");
                fclose(f);
                return 1;
            }
        } else {
            lines++;
        }
    }
    fclose(f);
    printf("lines: %d\n", lines);
    return 0;
}
