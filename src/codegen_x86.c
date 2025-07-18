#include <stdio.h>
#include "codegen_x86.h"
#include "regalloc_x86.h"

const char *x86_reg_str(int reg, asm_syntax_t syntax)
{
    const char *name = regalloc_reg_name(reg);
    if (syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
}

const char *x86_fmt_reg(const char *name, asm_syntax_t syntax)
{
    if (syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
}

const char *x86_loc_str(char buf[32], regalloc_t *ra, int id, int x64,
                        asm_syntax_t syntax)
{
    if (!ra || id <= 0)
        return "";
    int loc = ra->loc[id];
    if (loc >= 0)
        return x86_reg_str(loc, syntax);
    if (x64) {
        if (syntax == ASM_INTEL)
            snprintf(buf, 32, "[rbp-%d]", -loc * 8);
        else
            snprintf(buf, 32, "-%d(%%rbp)", -loc * 8);
    } else {
        if (syntax == ASM_INTEL)
            snprintf(buf, 32, "[ebp-%d]", -loc * 4);
        else
            snprintf(buf, 32, "-%d(%%ebp)", -loc * 4);
    }
    return buf;
}

void x86_emit_mov(strbuf_t *sb, const char *sfx,
                  const char *src, const char *dest,
                  asm_syntax_t syntax)
{
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest, src);
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, src, dest);
}

void x86_emit_op(strbuf_t *sb, const char *op, const char *sfx,
                 const char *src, const char *dest,
                 asm_syntax_t syntax)
{
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx, dest, src);
    else
        strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx, src, dest);
}

