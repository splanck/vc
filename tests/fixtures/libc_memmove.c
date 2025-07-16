#include <stdio.h>
#include <string.h>

int main(void) {
    char buf1[16] = "abcdef";
    memmove(buf1 + 2, buf1, 6);
    puts(buf1);
    char buf2[16] = "abcdef";
    memmove(buf2, buf2 + 1, 6);
    puts(buf2);
    return 0;
}
