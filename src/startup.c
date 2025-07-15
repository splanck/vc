#define _POSIX_C_SOURCE 200809L
/*
 * Startup helper routines.
 *
 * Contains helpers for emitting and assembling the program entry stub.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cli.h"
#include "command.h"
#include "startup.h"
#include "util.h"

/* Use binary mode for temporary files on platforms that require it */
#if defined(_WIN32)
# define TEMP_FOPEN_MODE "wb"
#else
# define TEMP_FOPEN_MODE "w"
#endif

const char *get_cc(void);
const char *get_as(int intel);

/* Write the entry stub assembly to a temporary file. */
int write_startup_asm(int use_x86_64, asm_syntax_t syntax,
                      const cli_options_t *cli, char **out_path)
{
    char *asmname = NULL;
    int asmfd = create_temp_file(cli, "vcstub", &asmname);
    if (asmfd < 0)
        return 0;
    FILE *stub = fdopen(asmfd, TEMP_FOPEN_MODE);
    if (!stub) {
        perror("fdopen");
        close(asmfd);
        unlink(asmname);
        free(asmname);
        return 0;
    }
    int rc;
    if (syntax == ASM_INTEL) {
        if (use_x86_64) {
            rc = fputs(
                "global _start\n"
                "_start:\n"
                "    mov rbx, rsp\n"
                "    mov rdi, [rbx]\n"
                "    lea rsi, [rbx+8]\n"
                "    lea rdx, [rsi+rdi*8+8]\n"
                "    and rsp, -16\n"
                "    lea rbp, [rel after_main]\n"
                "    call main\n"
                "after_main:\n"
                "    mov rdi, rax\n"
                "    mov rax, 60\n"
                "    syscall\n",
                stub);
        } else {
            rc = fputs(
                "global _start\n"
                "_start:\n"
                "    pop eax\n"
                "    mov ecx, esp\n"
                "    lea edx, [ecx+eax*4+4]\n"
                "    and esp, -16\n"
                "    push edx\n"
                "    push ecx\n"
                "    push eax\n"
                "    call main\n"
                "    mov ebx, eax\n"
                "    mov eax, 1\n"
                "    int 0x80\n",
                stub);
        }
    } else {
        if (use_x86_64) {
            rc = fputs(
                ".globl _start\n"
                "_start:\n"
                "    mov %rsp, %rbx\n"
                "    mov (%rbx), %rdi\n"
                "    lea 8(%rbx), %rsi\n"
                "    lea 8(%rsi,%rdi,8), %rdx\n"
                "    and $-16, %rsp\n"
                "    lea after_main(%rip), %rbp\n"
                "    call main\n"
                "after_main:\n"
                "    mov %rax, %rdi\n"
                "    mov $60, %rax\n"
                "    syscall\n",
                stub);
        } else {
            rc = fputs(
                ".globl _start\n"
                "_start:\n"
                "    pop %eax\n"
                "    mov %esp, %ecx\n"
                "    lea 4(%ecx,%eax,4), %edx\n"
                "    and $-16, %esp\n"
                "    push %edx\n"
                "    push %ecx\n"
                "    push %eax\n"
                "    call main\n"
                "    mov %eax, %ebx\n"
                "    mov $1, %eax\n"
                "    int $0x80\n",
                stub);
        }
    }
    if (rc == EOF) {
        perror("fputs");
        fclose(stub);
        unlink(asmname);
        free(asmname);
        return 0;
    }
    if (fclose(stub) == EOF) {
        perror("fclose");
        unlink(asmname);
        free(asmname);
        return 0;
    }

    *out_path = asmname;
    return 1;
}

/* Assemble the entry stub into an object file. */
int assemble_startup_obj(const char *asm_path, int use_x86_64,
                         const cli_options_t *cli, char **out_path)
{
    char *objname = NULL;
    int objfd = create_temp_file(cli, "vcobj", &objname);
    if (objfd < 0)
        return 0;
    close(objfd);

    int rc;
    if (cli->asm_syntax == ASM_INTEL) {
        const char *fmt = use_x86_64 ? "elf64" : "elf32";
        char *argv[] = {(char *)get_as(1), "-f", (char *)fmt,
                        (char *)asm_path, "-o", objname, NULL};
        rc = command_run(argv);
    } else {
        const char *arch_flag = use_x86_64 ? "-m64" : "-m32";
        char *argv[] = {(char *)get_as(0), "-x", "assembler", (char *)arch_flag,
                        "-c", (char *)asm_path, "-o", objname, NULL};
        rc = command_run(argv);
    }
    if (rc != 1) {
        if (rc == 0)
            fprintf(stderr, "assembly failed\n");
        else if (rc == -1)
            fprintf(stderr, "assembler terminated by signal\n");
        unlink(objname);
        free(objname);
        return 0;
    }

    *out_path = objname;
    return 1;
}
