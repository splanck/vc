/*
 * High level translation of the IR to x86 assembly.
 *
 * Each instruction emitted by the IR builder is dispatched to one of the
 * specialised emitters found in the codegen_* modules.  Those helpers use
 * register allocation information to determine whether operands live in
 * registers or on the stack and output the appropriate assembly.  Both
 * 32- and 64-bit variants are supported.  The `x64` parameter selects
 * which register set and instruction suffixes are used.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "codegen.h"
#include "cli.h"
#include "regalloc.h"
#include "regalloc_x86.h"
#include "strbuf.h"
#include "label.h"
#include "codegen_mem.h"
#include "codegen_arith.h"
#include "codegen_branch.h"

/*
 * Global flags controlling optional assembly output.
 *
 * `export_syms` determines whether function labels are emitted with
 * `.globl` so that they can be linked from other objects.  Valid values
 * are 0 (do not export) or 1 (export all functions).
 *
 * `debug_info` toggles emission of `.file` and `.loc` directives used by
 * debuggers.  Set to 1 to enable them or 0 to omit the directives.
 */
int export_syms = 0;
static int debug_info = 0;
int dwarf_enabled = 0;

/*
 * Enable or disable symbol export.
 *
 * When enabled the prologue emitter in codegen_branch.c will mark
 * functions with `.globl` so that the resulting object exposes the
 * symbol.  This mirrors the behaviour of a public function in C.
 */
void codegen_set_export(int flag)
{
    export_syms = flag;
}

/* Enable or disable emission of debug info */
void codegen_set_debug(int flag)
{
    debug_info = flag;
}

/* Enable or disable DWARF output */
void codegen_set_dwarf(int flag)
{
    dwarf_enabled = flag;
}



/*
 * Dispatch a single IR instruction to the specialised emitter.
 *
 * The opcode determines which helper handles the instruction.  The
 * register allocator state `ra` provides operand locations and `x64`
 * selects the 32- or 64-bit instruction forms used by the helper.
 */
static void emit_instr(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64,
                       asm_syntax_t syntax)
{
    switch (ins->op) {
    case IR_CONST: case IR_LOAD: case IR_STORE:
    case IR_LOAD_PARAM: case IR_STORE_PARAM:
    case IR_ADDR: case IR_LOAD_PTR: case IR_STORE_PTR:
    case IR_LOAD_IDX: case IR_STORE_IDX:
    case IR_BFLOAD: case IR_BFSTORE:
    case IR_ARG: case IR_GLOB_STRING: case IR_GLOB_WSTRING:
    case IR_GLOB_VAR: case IR_GLOB_ARRAY:
    case IR_GLOB_UNION: case IR_GLOB_STRUCT:
        emit_memory_instr(sb, ins, ra, x64, syntax);
        break;

    case IR_PTR_ADD: case IR_PTR_DIFF:
    case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
    case IR_LFADD: case IR_LFSUB: case IR_LFMUL: case IR_LFDIV:
    case IR_CPLX_ADD: case IR_CPLX_SUB:
    case IR_CPLX_MUL: case IR_CPLX_DIV:
    case IR_ADD: case IR_SUB: case IR_MUL:
    case IR_DIV: case IR_MOD: case IR_SHL:
    case IR_SHR: case IR_AND: case IR_OR:
    case IR_XOR: case IR_CAST:
    case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT: case IR_CMPGT:
    case IR_CMPLE: case IR_CMPGE:
    case IR_LOGAND: case IR_LOGOR:
        emit_arith_instr(sb, ins, ra, x64, syntax);
        break;

    default:
        emit_branch_instr(sb, ins, ra, x64, syntax);
        break;
    }
}

/*
 * Translate the IR instruction stream to x86 assembly and return it.
 *
 * Register allocation is performed before walking the list of IR
 * instructions.  Each IR opcode is lowered with `emit_instr`, producing
 * either 32- or 64-bit mnemonics depending on the `x64` flag.  Global
 * declarations are not included in the returned buffer.  The caller takes
 * ownership of the heap-allocated string.
 */
char *codegen_ir_to_string(ir_builder_t *ir, int x64,
                           asm_syntax_t syntax)
{
    if (!ir)
        return NULL;
    regalloc_t ra;
    regalloc_set_x86_64(x64);
    regalloc_set_asm_syntax(syntax);
    regalloc_run(ir, &ra);
    regalloc_xmm_reset();

    strbuf_t sb;
    strbuf_init(&sb);
    if (debug_info && ir->head && ir->head->file)
        strbuf_appendf(&sb, ".file 1 \"%s\"\n", ir->head->file);
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (debug_info && ins->file && ins->line)
            strbuf_appendf(&sb, ".loc 1 %zu %zu\n", ins->line, ins->column);
        emit_instr(&sb, ins, &ra, x64, syntax);
    }

    regalloc_free(&ra);
    return sb.data; /* caller takes ownership */
}

/*
 * Emit the assembly representation of `ir` to the stream `out`.
 *
 * Global data is written first using `.data` directives.  The instruction
 * sequence is then converted to either 32- or 64-bit x86 via
 * `codegen_ir_to_string` and emitted after a `.text` header when needed.
 * The `x64` argument selects the target word size.
 */
void codegen_emit_x86(FILE *out, ir_builder_t *ir, int x64,
                      asm_syntax_t syntax)
{
    if (!out)
        return;
    const char *size_directive = x64 ? ".quad" : ".long";
    int has_data = 0;
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (ins->op == IR_GLOB_VAR || ins->op == IR_GLOB_STRING ||
            ins->op == IR_GLOB_WSTRING ||
            ins->op == IR_GLOB_ARRAY || ins->op == IR_GLOB_UNION ||
            ins->op == IR_GLOB_STRUCT) {
            if (!has_data) {
                fputs(".data\n", out);
                has_data = 1;
            }
            if (ins->src1)
                fprintf(out, ".local %s\n", ins->name);
            fprintf(out, "%s:\n", ins->name);
            if (ins->op == IR_GLOB_VAR) {
                fprintf(out, "    %s %lld\n", size_directive, ins->imm);
            } else if (ins->op == IR_GLOB_STRING) {
                fprintf(out, "    .asciz \"%s\"\n", ins->data);
            } else if (ins->op == IR_GLOB_WSTRING) {
                long long *vals = (long long *)ins->data;
                for (long long i = 0; i < ins->imm; i++)
                    fprintf(out, "    %s %lld\n", size_directive, vals[i]);
            } else if (ins->op == IR_GLOB_ARRAY) {
                long long *vals = (long long *)ins->data;
                for (long long i = 0; i < ins->imm; i++)
                    fprintf(out, "    %s %lld\n", size_directive, vals[i]);
            } else if (ins->op == IR_GLOB_UNION || ins->op == IR_GLOB_STRUCT) {
                fprintf(out, "    .zero %lld\n", ins->imm);
            }
        }
    }
    if (has_data)
        fputs(".text\n", out);

    char *text = codegen_ir_to_string(ir, x64, syntax);
    if (text) {
        fputs(text, out);
        free(text);
    }

    if (dwarf_enabled)
        fputs(".section .debug_info\n    .byte 0\n", out);
}

