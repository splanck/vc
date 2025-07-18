#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "util.h"
#include "vector.h"
#include "command.h"
#include "compile.h"
#include "startup.h"
#include "cli.h"
#include "compile_helpers.h"

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

    if (len > SIZE_MAX - 3) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    char *obj = malloc(len + 3);
    if (!obj)
        return NULL;

    memcpy(obj, base, len);
    memcpy(obj + len, ".o", 3);
    return obj;
}

/* Return a dependency file name derived from TARGET */
#ifdef UNIT_TESTING
char *vc_dep_name(const char *target)
#else
static char *vc_dep_name(const char *target)
#endif
{
    const char *base = strrchr(target, '/');
    base = base ? base + 1 : target;
    const char *dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);

    if (len > SIZE_MAX - 3) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    char *dep = malloc(len + 3);
    if (!dep)
        return NULL;
    memcpy(dep, base, len);
    memcpy(dep + len, ".d", 3);
    return dep;
}

static int
fputs_make_escaped(FILE *f, const char *s)
{
    for (; *s; s++) {
        switch (*s) {
        case ' ':
        case '\t':
        case '#':
        case '$':
        case '\\':
            if (fputc('\\', f) == EOF)
                return 0;
            break;
        default:
            break;
        }
        if (fputc(*s, f) == EOF)
            return 0;
    }
    return 1;
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
    int ok = fputs_make_escaped(f, target);
    if (ok)
        ok = (fputc(':', f) != EOF);
    for (size_t i = 0; i < deps->count && ok; i++) {
        if (fputc(' ', f) == EOF)
            ok = 0;
        else
            ok = fputs_make_escaped(f, ((const char **)deps->data)[i]);
    }
    if (ok)
        ok = (fputc('\n', f) != EOF);
    if (fclose(f) != 0)
        ok = 0;
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
        perror("mkostemp");
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

/* Push a compiled object name into the vector. */
static int push_object(vector_t *objs, char *obj)
{
    if (!vector_push(objs, &obj)) {
        vc_oom();
        unlink(obj);
        free(obj);
        return 0;
    }
    return 1;
}

/* Compile all CLI sources and append objects to the vector. */
static int compile_all_sources(const cli_options_t *cli, vector_t *objs)
{
    for (size_t i = 0; i < cli->sources.count; i++) {
        const char *src = ((const char **)cli->sources.data)[i];
        char *obj = NULL;
        if (!compile_source_obj(src, cli, &obj))
            return 0;
        if (!push_object(objs, obj))
            return 0;
    }
    return 1;
}

/* Remove object files on failure and reset the vector. */
static void cleanup_object_vector(vector_t *objs)
{
    for (size_t j = 0; j < objs->count; j++) {
        unlink(((char **)objs->data)[j]);
        free(((char **)objs->data)[j]);
    }
    objs->count = 0;
}

/* Compile all sources to temporary object files. */
static int compile_source_files(const cli_options_t *cli, vector_t *objs)
{
    vector_init(objs, sizeof(char *));

    int ok = compile_all_sources(cli, objs);
    if (!ok)
        cleanup_object_vector(objs);

    return ok;
}

/* Allocate and populate the argument vector for the linker command. */
static char **
build_linker_args(const vector_t *objs, const vector_t *lib_dirs,
                  const vector_t *libs, const char *output, int use_x86_64)
{
    const char *arch_flag = use_x86_64 ? "-m64" : "-m32";

    /* base linker arguments:
     *   0: compiler command (cc)
     *   1: architecture flag (-m32/-m64)
     *   2: -no-pie
     *   3: -nostdlib
     *   4: -o
     *   5: output file path
     */
    const char *base[] = {
        get_cc(), arch_flag, "-no-pie", "-nostdlib", "-o", output
    };
    size_t base_args = sizeof(base) / sizeof(base[0]);

    /* calculate required argument count and detect overflow */
    size_t argc = base_args;
    if (objs->count > SIZE_MAX - argc)
        goto arg_overflow;
    argc += objs->count;
    if (lib_dirs->count > (SIZE_MAX - argc) / 2)
        goto arg_overflow;
    argc += lib_dirs->count * 2;
    if (libs->count > (SIZE_MAX - argc) / 2)
        goto arg_overflow;
    argc += libs->count * 2;

    size_t n = argc + 1; /* plus NULL terminator */
    if (n > SIZE_MAX / sizeof(char *))
        goto arg_overflow;

    char **argv = vc_alloc_or_exit(n * sizeof(char *));

    size_t idx = 0;
    argv[idx++] = (char *)base[0];
    argv[idx++] = (char *)base[1];
    argv[idx++] = (char *)base[2];
    for (size_t i = 0; i < objs->count; i++)
        argv[idx++] = ((char **)objs->data)[i];
    for (size_t i = 0; i < lib_dirs->count; i++) {
        argv[idx++] = "-L";
        argv[idx++] = ((char **)lib_dirs->data)[i];
    }
    argv[idx++] = (char *)base[3];
    for (size_t i = 0; i < libs->count; i++) {
        argv[idx++] = "-l";
        argv[idx++] = ((char **)libs->data)[i];
    }
    argv[idx++] = (char *)base[4];
    argv[idx++] = (char *)base[5];
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

/* Populate library directory and library name vectors from CLI options. */
static int
prepare_lib_vectors(const cli_options_t *cli, vector_t *lib_dirs,
                    vector_t *libs, vector_t *dup_dirs)
{
    vector_init(lib_dirs, sizeof(char *));
    vector_init(libs, sizeof(char *));
    vector_init(dup_dirs, sizeof(char *));

    for (size_t i = 0; i < cli->lib_dirs.count; i++) {
        char *dir = ((char **)cli->lib_dirs.data)[i];
        if (!vector_push(lib_dirs, &dir)) {
            vc_oom();
            vector_free(lib_dirs);
            vector_free(libs);
            free_string_vector(dup_dirs);
            return 0;
        }
    }

    for (size_t i = 0; i < cli->libs.count; i++) {
        char *lib = ((char **)cli->libs.data)[i];
        if (!vector_push(libs, &lib)) {
            vc_oom();
            vector_free(lib_dirs);
            vector_free(libs);
            free_string_vector(dup_dirs);
            return 0;
        }
    }

    return 1;
}

/* Add the internal libc to the provided vectors when requested. */
static int
add_internal_libc(vector_t *lib_dirs, vector_t *libs, vector_t *dup_dirs,
                  const cli_options_t *cli)
{
    const char *inc = (cli->vc_sysinclude && *cli->vc_sysinclude)
                          ? cli->vc_sysinclude
                          : PROJECT_ROOT "/libc/include";
    char base[PATH_MAX];
    int ret = snprintf(base, sizeof(base), "%s", inc);
    if (ret < 0) {
        fprintf(stderr, "vc: failed to format internal libc path\n");
        return 0;
    }
    if (ret >= (int)sizeof(base)) {
        fprintf(stderr, "vc: internal libc path too long\n");
        return 0;
    }
    char *slash = strrchr(base, '/');
    if (slash)
        *slash = '\0';

    const char *libname = cli->use_x86_64 ? "c64" : "c32";
    char archive[PATH_MAX];
    int n = snprintf(archive, sizeof(archive), "%s/lib%s.a", base,
                     cli->use_x86_64 ? "c64" : "c32");
    if (n < 0) {
        fprintf(stderr,
                "vc: failed to format internal libc archive path\n");
        return 0;
    }
    if (n >= (int)sizeof(archive)) {
        fprintf(stderr, "vc: internal libc archive path too long\n");
        return 0;
    }

    if (access(archive, F_OK) != 0) {
        const char *make_target = cli->use_x86_64 ? "libc64" : "libc32";
        fprintf(stderr,
                "vc: internal libc archive '%s' not found. Build it with 'make %s'\n",
                archive, make_target);
        return 0;
    }

    char *dir_dup = vc_strdup(base);
    if (!dir_dup) {
        vc_oom();
        return 0;
    }
    if (!vector_push(lib_dirs, &dir_dup) ||
        !vector_push(dup_dirs, &dir_dup)) {
        free(dir_dup);
        vc_oom();
        return 0;
    }
    if (!vector_push(libs, &libname)) {
        vc_oom();
        return 0;
    }

    return 1;
}

/* Link all object files and release library vectors. */
static int
link_final_objects(vector_t *objs, vector_t *lib_dirs, vector_t *libs,
                   vector_t *dup_dirs, const cli_options_t *cli)
{
    int ok = run_link_command(objs, lib_dirs, libs,
                              cli->output, cli->use_x86_64);
    vector_free(lib_dirs);
    vector_free(libs);
    free_string_vector(dup_dirs);
    return ok;
}

/* Create entry stub and link all objects into the final executable. */
#ifdef UNIT_TESTING
int build_and_link_objects(vector_t *objs, const cli_options_t *cli)
#else
static int build_and_link_objects(vector_t *objs, const cli_options_t *cli)
#endif
{
    /* Phase 1: create startup object */
    char *stubobj = NULL;
    int ok = create_startup_object(cli, cli->use_x86_64, &stubobj);
    if (!ok)
        return 0;

    if (!vector_push(objs, &stubobj)) {
        vc_oom();
        unlink(stubobj);
        free(stubobj);
        return 0;
    }

    /* Phase 2: detect -nostdlib flag */
    int disable_stdlib = 0;
    for (size_t i = 0; i < cli->libs.count; i++) {
        const char *lib = ((const char **)cli->libs.data)[i];
        if (strcmp(lib, "nostdlib") == 0) {
            disable_stdlib = 1;
            break;
        }
    }

    /* Phase 3: gather library options from CLI */
    vector_t lib_dirs;
    vector_t libs;
    vector_t dup_dirs; /* track heap allocations for internal libc */
    if (!prepare_lib_vectors(cli, &lib_dirs, &libs, &dup_dirs))
        return 0;

    /* Phase 4: optionally add internal libc */
    if (cli->internal_libc && !disable_stdlib) {
        if (!add_internal_libc(&lib_dirs, &libs, &dup_dirs, cli)) {
            vector_free(&lib_dirs);
            vector_free(&libs);
            free_string_vector(&dup_dirs);
            return 0;
        }
    }

    /* Phase 5: link into final executable */
    return link_final_objects(objs, &lib_dirs, &libs, &dup_dirs, cli);
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

