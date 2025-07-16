#include <stdio.h>
int main(void) {
    FILE *tmp = tmpfile();
    if (!tmp)
        return 1;
    if (fprintf(tmp, "hello") < 0)
        return 1;
    fclose(tmp);
    return 0;
}
