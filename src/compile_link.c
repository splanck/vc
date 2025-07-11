#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "util.h"
#include "vector.h"
#include "command.h"
#include "compile.h"
#include "startup.h"
#include "cli.h"

/* external helpers from other compilation units */
int create_temp_file(const cli_options_t *cli, const char *prefix,
                     char **out_path);
const char *get_cc(void);

/*
 * Return a newly allocated object file name for the given source path.
 * The caller must free the returned string.  NULL is returned on memory
 * allocation failure.
 */
char *
vc_obj_name(const char *source)
{
    const char *base = strrchr(source, '/');
    base = base ? base + 1 : source;
    const char *dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);

    char *obj = malloc(len + 3);
    if (!obj)
        return NULL;

    memcpy(obj, base, len);
    strcpy(obj + len, ".o");
    return obj;
}

/* Return a dependency file name derived from TARGET */
static char *vc_dep_name(const char *target)
{
    const char *base = strrchr(target, '/');
    base = base ? base + 1 : target;
    const char *dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);
    char *dep = malloc(len + 3);
    if (!dep)
        return NULL;
    memcpy(dep, base, len);
    strcpy(dep + len, ".d");
    return dep;
}

int write_dep_file(const char *target, const vector_t *deps)
{
    char *dep = vc_dep_name(target);
    if (!dep) {
        vc_oom();
        return 0;
    }
    FILE *f = fopen(dep, "w");
    if (!f) {
        perror(dep);
        free(dep);
        return 0;
    }
    fprintf(f, "%s:", target);
    for (size_t i = 0; i < deps->count; i++)
        fprintf(f, " %s", ((const char **)deps->data)[i]);
    fprintf(f, "\n");
    int ok = (fclose(f) == 0);
    if (!ok)
        perror(dep);
    free(dep);
    return ok;
}

/* Create object file with program entry point */
static int create_startup_object(const cli_options_t *cli, int use_x86_64,
                                char **out_path)
{
    char *asmfile = NULL;
    int ok = write_startup_asm(use_x86_64, cli->asm_syntax, cli, &asmfile);
    if (ok)
        ok = assemble_startup_obj(asmfile, use_x86_64, cli, out_path);
    if (asmfile) {
        unlink(asmfile);
        free(asmfile);
    }
    return ok;
}

/* Compile a single source file to a temporary object. */
#ifdef UNIT_TESTING
int compile_source_obj(const char *source, const cli_options_t *cli,
                       char **out_path)
#else
static int compile_source_obj(const char *source, const cli_options_t *cli,
                              char **out_path)
#endif
{
    char *objname = NULL;
    int fd = create_temp_file(cli, "vcobj", &objname);
    if (fd < 0) {
        perror("mkstemp");
        return 0;
    }
    close(fd);

    int ok = compile_unit(source, cli, objname, 1);
    if (!ok) {
        unlink(objname);
        free(objname);
        return 0;
    }

    *out_path = objname;
    return 1;
}

/* Compile all sources to temporary object files. */
static int compile_source_files(const cli_options_t *cli, vector_t *objs)
{
    int ok = 1;
    vector_init(objs, sizeof(char *));

    for (size_t i = 0; i < cli->sources.count; i++) {
        const char *src = ((const char **)cli->sources.data)[i];
        char *obj = NULL;

        if (!compile_source_obj(src, cli, &obj)) {
            ok = 0;
            break;
        }

        if (!vector_push(objs, &obj)) {
            vc_oom();
            ok = 0;
            unlink(obj);
            free(obj);
            break;
        }
    }

    if (!ok) {
        for (size_t j = 0; j < objs->count; j++) {
            unlink(((char **)objs->data)[j]);
            free(((char **)objs->data)[j]);
        }
        objs->count = 0;
    }

    return ok;
}

/* Allocate and populate the argument vector for the linker command. */
static char **
build_linker_args(const vector_t *objs, const vector_t *lib_dirs,
                  const vector_t *libs, const char *output, int use_x86_64)
{
    const char *arch_flag = use_x86_64 ? "-m64" : "-m32";

    /* calculate required argument count and detect overflow */
    size_t argc = 0;
    if (objs->count > SIZE_MAX - argc)
        goto arg_overflow;
    argc += objs->count;
    if (lib_dirs->count > (SIZE_MAX - argc) / 2)
        goto arg_overflow;
    argc += lib_dirs->count * 2;
    if (libs->count > (SIZE_MAX - argc) / 2)
        goto arg_overflow;
    argc += libs->count * 2;
    if (5 > SIZE_MAX - argc)
        goto arg_overflow;
    argc += 5;

    size_t n = argc + 1; /* plus NULL terminator */
    if (n > SIZE_MAX / sizeof(char *))
        goto arg_overflow;

    char **argv = vc_alloc_or_exit(n * sizeof(char *));

    size_t idx = 0;
    argv[idx++] = (char *)get_cc();
    argv[idx++] = (char *)arch_flag;
    for (size_t i = 0; i < objs->count; i++)
        argv[idx++] = ((char **)objs->data)[i];
    for (size_t i = 0; i < lib_dirs->count; i++) {
        argv[idx++] = "-L";
        argv[idx++] = ((char **)lib_dirs->data)[i];
    }
    argv[idx++] = "-nostdlib";
    for (size_t i = 0; i < libs->count; i++) {
        argv[idx++] = "-l";
        argv[idx++] = ((char **)libs->data)[i];
    }
    argv[idx++] = "-o";
    argv[idx++] = (char *)output;
    argv[idx] = NULL;
    return argv;

arg_overflow:
    fprintf(stderr, "vc: argument vector too large\n");
    return NULL;
}

/* Free argument vector returned by build_linker_args. */
static void free_linker_args(char **argv)
{
    free(argv);
}

/* Construct and run the final cc link command. */
static int run_link_command(const vector_t *objs, const vector_t *lib_dirs,
                            const vector_t *libs, const char *output,
                            int use_x86_64)
{
    char **argv = build_linker_args(objs, lib_dirs, libs, output,
                                    use_x86_64);
    if (!argv)
        return 0;

    int rc = command_run(argv);
    free_linker_args(argv);
    if (rc != 1) {
        if (rc == 0)
            fprintf(stderr, "linker failed\n");
        else if (rc == -1)
            fprintf(stderr, "linker terminated by signal\n");
        return 0;
    }
    return 1;
}

/* Create entry stub and link all objects into the final executable. */
static int build_and_link_objects(vector_t *objs, const cli_options_t *cli)
{
    char *stubobj = NULL;
    int ok = create_startup_object(cli, cli->use_x86_64, &stubobj);
    if (ok) {
        if (!vector_push(objs, &stubobj)) {
            vc_oom();
            unlink(stubobj);
            free(stubobj);
            return 0;
        }
        ok = run_link_command(objs, &cli->lib_dirs, &cli->libs,
                              cli->output, cli->use_x86_64);
    }
    return ok;
}

/* Compile all sources and link them into the final executable. */
int link_sources(const cli_options_t *cli)
{
    vector_t objs;
    int ok = compile_source_files(cli, &objs);

    if (ok)
        ok = build_and_link_objects(&objs, cli);

    for (size_t i = 0; i < objs.count; i++) {
        unlink(((char **)objs.data)[i]);
        free(((char **)objs.data)[i]);
    }
    vector_free(&objs);

    return ok;
}

