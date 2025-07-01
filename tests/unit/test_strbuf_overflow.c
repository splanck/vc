#include <stddef.h>
#include <stdint.h>
#include "strbuf.h"

int main(void)
{
    strbuf_t sb;
    strbuf_init(&sb);
    sb.len = SIZE_MAX - 16; /* force near-overflow */
    sb.cap = 1;
    if (strbuf_append(&sb, "test") == 0)
        return 1;
    return 0;
}
