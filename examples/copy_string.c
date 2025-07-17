#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    const char *src = "test string";
    size_t len = strlen(src) + 1;
    char *dup = malloc(len);
    if (!dup) {
        perror("malloc");
        return 1;
    }
    memcpy(dup, src, len);
    printf("%s (%d chars)\n", dup, (int)(len - 1));
    free(dup);
    return 0;
}
