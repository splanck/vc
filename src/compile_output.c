#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "util.h"
#include "cli.h"
#include "ir_core.h"
#include "ir_dump.h"
#include "codegen.h"
#include "command.h"
#include "compile_helpers.h"

#if defined(_WIN32)
# define TEMP_FOPEN_MODE "wb"
#else
# define TEMP_FOPEN_MODE "w"
#endif



const char *
get_cc(void)
{
    const char *cc = getenv("CC");
    return (cc && *cc) ? cc : "cc";
}

const char *
get_as(int intel)
{
    const char *as = getenv("AS");
    if (as && *as)
        return as;
    return intel ? "nasm" : "cc";
}


static const char nasm_macros[] =
    "%macro movl 2\n    mov %1, %2\n%endmacro\n"
    "%macro movq 2\n    mov %1, %2\n%endmacro\n"
    "%macro addl 2\n    add %1, %2\n%endmacro\n"
    "%macro addq 2\n    add %1, %2\n%endmacro\n"
    "%macro subl 2\n    sub %1, %2\n%endmacro\n"
    "%macro subq 2\n    sub %1, %2\n%endmacro\n"
    "%macro imull 2\n    imul %1, %2\n%endmacro\n"
    "%macro imulq 2\n    imul %1, %2\n%endmacro\n"
    "%macro cmpl 2\n    cmp %1, %2\n%endmacro\n"
    "%macro cmpq 2\n    cmp %1, %2\n%endmacro\n"
    "%macro leal 2\n    lea %1, %2\n%endmacro\n"
    "%macro leaq 2\n    lea %1, %2\n%endmacro\n"
    "%macro pushl 1\n    push %1\n%endmacro\n"
    "%macro pushq 1\n    push %1\n%endmacro\n"
    "%macro popl 1\n    pop %1\n%endmacro\n"
    "%macro popq 1\n    pop %1\n%endmacro\n";

/*
 * Write the generated assembly to a temporary file.  The caller must
 * unlink and free the returned path on success.  Errors are reported to
 * stderr and the temporary file is cleaned up before returning 0.
 */
static int write_assembly_file(ir_builder_t *ir, int use_x86_64,
                               const cli_options_t *cli, char **out_path)
{
    char *tmpname = NULL;
    int fd = create_temp_file(cli, "vc", &tmpname);
    if (fd < 0)
        return 0;

    FILE *tmpf = fdopen(fd, TEMP_FOPEN_MODE);
    if (!tmpf) {
        perror("fdopen");
        close(fd);
        unlink(tmpname);
        free(tmpname);
        return 0;
    }

    if (cli->asm_syntax == ASM_INTEL) {
        if (fputs(nasm_macros, tmpf) == EOF) {
            perror("fputs");
            fclose(tmpf);
            unlink(tmpname);
            free(tmpname);
            return 0;
        }
    }

    codegen_emit_x86(tmpf, ir, use_x86_64, cli->asm_syntax);

    if (fflush(tmpf) == EOF) {
        perror("fflush");
        fclose(tmpf);
        unlink(tmpname);
        free(tmpname);
        return 0;
    }
    if (fclose(tmpf) == EOF) {
        perror("fclose");
        unlink(tmpname);
        free(tmpname);
        return 0;
    }

    *out_path = tmpname;
    return 1;
}

/*
 * Assemble ASMFILE into OUTPUT using the selected assembler.  Any
 * command execution failures are reported to stderr and 0 is returned.
 */
static int invoke_assembler(const char *asmfile, const char *output,
                            int use_x86_64, const cli_options_t *cli)
{
    int rc;
    if (cli->asm_syntax == ASM_INTEL) {
        const char *fmt = use_x86_64 ? "elf64" : "elf32";
        char *argv[] = {(char *)get_as(1), "-f", (char *)fmt, (char *)asmfile,
                        "-o", (char *)output, NULL};
        rc = command_run(argv);
    } else {
        const char *arch_flag = use_x86_64 ? "-m64" : "-m32";
        char *argv[] = {(char *)get_as(0), "-x", "assembler", (char *)arch_flag,
                        "-c", (char *)asmfile, "-o", (char *)output, NULL};
        rc = command_run(argv);
    }
    if (rc != 1) {
        if (rc == 0)
            fprintf(stderr, "assembly failed\n");
        else if (rc == -1)
            fprintf(stderr, "assembler terminated by signal\n");
        return 0;
    }
    return 1;
}

static int emit_output_file(ir_builder_t *ir, const char *output,
                            int use_x86_64, int compile_obj,
                            const cli_options_t *cli)
{
    if (compile_obj) {
        /*
         * Write assembly to a temporary file and run the assembler.
         * The temporary file is removed regardless of success.  Any
         * errors are reported by the helper routines.
         */
        char *asmfile = NULL;
        int ok = write_assembly_file(ir, use_x86_64, cli, &asmfile);
        if (ok) {
            ok = invoke_assembler(asmfile, output, use_x86_64, cli);
            unlink(asmfile);
            free(asmfile);
        }
        return ok;
    }

    FILE *outf = fopen(output, "wb");
    if (!outf) {
        perror("fopen");
        return 0;
    }
    codegen_emit_x86(outf, ir, use_x86_64,
                    cli->asm_syntax);
    if (fclose(outf) == EOF) {
        perror("fclose");
        unlink(output);
        return 0;
    }
    return 1;
}

int compile_output_impl(ir_builder_t *ir, const char *output,
                        int dump_ir, int dump_asm, int use_x86_64,
                        int compile, const cli_options_t *cli)
{
    if (dump_ir) {
        char *text = ir_to_string(ir);
        if (text) {
            printf("%s", text);
            free(text);
        }
        return 1;
    }
    if (dump_asm) {
        char *text = codegen_ir_to_string(ir, use_x86_64,
                                          cli->asm_syntax);
        if (text) {
            printf("%s", text);
            free(text);
        }
        return 1;
    }

    return emit_output_file(ir, output, use_x86_64, compile, cli);
}

