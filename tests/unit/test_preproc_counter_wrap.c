#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include "preproc_builtin.h"
#include "strbuf.h"

int main(void)
{
    preproc_context_t ctx = {0};
    ctx.counter = UINT64_MAX;
    strbuf_t sb; strbuf_init(&sb);
    size_t pos = 0;
    handle_builtin_macro("__COUNTER__", 11, 11, 1, &sb, &pos, &ctx);
    strbuf_free(&sb);
    return ctx.counter == 0 ? 0 : 1;
}
