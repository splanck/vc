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
#include "vector.h"

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
    case IR_GLOB_UNION: case IR_GLOB_STRUCT: case IR_GLOB_ADDR:
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
 * Emit global declarations such as strings and arrays.
 *
 * The IR uses specialised opcodes to describe each global object. They
 * are translated here into `.data` directives.  The helper returns 1
 * when at least one directive is written so the caller knows whether a
 * `.text` header is needed before the instruction stream.
 */
static int emit_global_data(FILE *out, const ir_builder_t *ir, int x64)
{
    const char *size_directive = x64 ? ".quad" : ".long";
    int has_data = 0;

    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (ins->op == IR_GLOB_VAR || ins->op == IR_GLOB_STRING ||
            ins->op == IR_GLOB_WSTRING || ins->op == IR_GLOB_ARRAY ||
            ins->op == IR_GLOB_UNION || ins->op == IR_GLOB_STRUCT ||
            ins->op == IR_GLOB_ADDR) {
            if (!has_data) {
                fputs(".data\n", out);
                has_data = 1;
            }
            if (ins->src1)
                fprintf(out, ".local %s\n", ins->name);
            if (ins->src2 > 1)
                fprintf(out, "    .align %d\n", ins->src2);
            fprintf(out, "%s:\n", ins->name);
            if (ins->op == IR_GLOB_VAR) {
                fprintf(out, "    %s %lld\n", size_directive, ins->imm);
            } else if (ins->op == IR_GLOB_STRING) {
                const char *s = ins->data;
                fputs("    .asciz \"", out);
                for (; *s; s++) {
                    unsigned char c = (unsigned char)*s;
                    switch (c) {
                    case '\\': fputs("\\\\", out); break;
                    case '"':  fputs("\\\"", out); break;
                    case '\n': fputs("\\n", out); break;
                    case '\t': fputs("\\t", out); break;
                    case '\r': fputs("\\r", out); break;
                    case '\b': fputs("\\b", out); break;
                    case '\f': fputs("\\f", out); break;
                    case '\v': fputs("\\v", out); break;
                    case '\a': fputs("\\a", out); break;
                    default:
                        if (c < 32 || c > 126)
                            fprintf(out, "\\x%02x", c);
                        else
                            fputc(c, out);
                    }
                }
                fputs("\"\n", out);
            } else if (ins->op == IR_GLOB_WSTRING) {
                const long long *vals = (const long long *)ins->data;
                for (long long i = 0; i < ins->imm; i++)
                    fprintf(out, "    %s %lld\n", size_directive, vals[i]);
            } else if (ins->op == IR_GLOB_ARRAY) {
                const long long *vals = (const long long *)ins->data;
                for (long long i = 0; i < ins->imm; i++)
                    fprintf(out, "    %s %lld\n", size_directive, vals[i]);
            } else if (ins->op == IR_GLOB_UNION || ins->op == IR_GLOB_STRUCT) {
                fprintf(out, "    .zero %lld\n", ins->imm);
            } else if (ins->op == IR_GLOB_ADDR) {
                fprintf(out, "    %s %s\n", size_directive,
                        ins->data ? ins->data : "0");
            }
        }
    }

    return has_data;
}

/*
 * Emit zero-initialized storage for named local variables.
 *
 * When stack slots are not used the IR retains variable names in memory
 * operations.  Emit a `.lcomm` directive for each such name so that the
 * resulting assembly has a definition for every symbol referenced.
 */
static int emit_local_comm(FILE *out, const ir_builder_t *ir, int x64)
{
    vector_t names;
    vector_init(&names, sizeof(char *));

    vector_t globals;
    vector_init(&globals, sizeof(char *));

    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (ins->name && ins->name[0]) {
            if (strncmp(ins->name, "stack:", 6) == 0)
                continue;
            if (ins->op == IR_GLOB_VAR || ins->op == IR_GLOB_STRING ||
                ins->op == IR_GLOB_WSTRING || ins->op == IR_GLOB_ARRAY ||
                ins->op == IR_GLOB_UNION || ins->op == IR_GLOB_STRUCT ||
                ins->op == IR_GLOB_ADDR) {
                int exists = 0;
                for (size_t i = 0; i < globals.count && !exists; i++)
                    if (strcmp(((char **)globals.data)[i], ins->name) == 0)
                        exists = 1;
                if (!exists)
                    vector_push(&globals, &ins->name);
            }
        }
    }

    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        const char *name = ins->name;
        if (!name || !name[0] || strncmp(name, "tmp", 3) == 0)
            continue;
        if (strncmp(name, "stack:", 6) == 0)
            continue;

        if (ins->op == IR_GLOB_VAR || ins->op == IR_GLOB_STRING ||
            ins->op == IR_GLOB_WSTRING || ins->op == IR_GLOB_ARRAY ||
            ins->op == IR_GLOB_UNION || ins->op == IR_GLOB_STRUCT ||
            ins->op == IR_GLOB_ADDR || ins->op == IR_FUNC_BEGIN ||
            ins->op == IR_FUNC_END || ins->op == IR_LABEL ||
            ins->op == IR_BR || ins->op == IR_BCOND ||
            ins->op == IR_CALL || ins->op == IR_CALL_PTR ||
            ins->op == IR_CALL_NR || ins->op == IR_CALL_PTR_NR)
            continue;

        int is_global = 0;
        for (size_t i = 0; i < globals.count && !is_global; i++)
            if (strcmp(((char **)globals.data)[i], name) == 0)
                is_global = 1;
        if (is_global)
            continue;

        int exists = 0;
        for (size_t i = 0; i < names.count && !exists; i++)
            if (strcmp(((char **)names.data)[i], name) == 0)
                exists = 1;
        if (!exists)
            vector_push(&names, &name);
    }

    int emitted = 0;
    for (size_t i = 0; i < names.count; i++) {
        const char *n = ((char **)names.data)[i];
        if (!emitted)
            fputs(".bss\n", out);
        fprintf(out, ".lcomm %s, %d\n", n, x64 ? 8 : 4);
        emitted = 1;
    }

    vector_free(&names);
    vector_free(&globals);
    return emitted;
}

/*
 * Convert the instruction stream to text and write it to `out`.
 *
 * The heavy lifting is done by `codegen_ir_to_string`, which runs the
 * register allocator and lowers each instruction to assembly.  The
 * resulting string is printed and freed here.
 */
static void emit_text(FILE *out, ir_builder_t *ir, int x64,
                      asm_syntax_t syntax)
{
    char *text = codegen_ir_to_string(ir, x64, syntax);
    if (text) {
        fputs(text, out);
        free(text);
    }
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

    /* Stage 1: emit global and local data directives */
    int has_data = emit_global_data(out, ir, x64);
    int has_comm = emit_local_comm(out, ir, x64);
    if (has_data || has_comm)
        fputs(".text\n", out);

    /* Stage 2: emit the instruction stream */
    emit_text(out, ir, x64, syntax);

    /* Stage 3: optional DWARF section */
    if (dwarf_enabled)
        fputs(".section .debug_info\n    .byte 0\n", out);
}

