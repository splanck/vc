#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "preproc_path.h"
#include "cli_opts_env.h"
#include "util.h"
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int load_vcflags(int *argc, char ***argv, char ***out_argv,
                 char **out_buf)
{
    char **vcargv = NULL;
    char *vcbuf = NULL;
    size_t vcargc = 0;
    const char *env = getenv("VCFLAGS");
    if (env && *env) {
        vcbuf = vc_strdup(env);
        if (!vcbuf) {
            fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
            return 1;
        }

        char *tmp = vc_strdup(env);
        if (!tmp) {
            fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
            free(vcbuf);
            return 1;
        }
        for (char *t = strtok(tmp, " "); t; t = strtok(NULL, " "))
            vcargc++;
        free(tmp);

        size_t alloc_sz = sizeof(char *) * ((size_t)*argc + vcargc);
        vcargv = malloc(alloc_sz);
        if (!vcargv) {
            fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
            free(vcbuf);
            return 1;
        }

        vcargv[0] = (*argv)[0];
        size_t idx = 1;
        for (char *t = strtok(vcbuf, " "); t; t = strtok(NULL, " "))
            vcargv[idx++] = t;
        for (int i = 1; i < *argc; i++)
            vcargv[idx++] = (*argv)[i];

        *argv = vcargv;
        *argc += (int)vcargc;
    }

    *out_argv = vcargv;
    *out_buf = vcbuf;
    return 0;
}

void scan_shortcuts(int *argc, char **argv)
{
    int new_argc = 1;
    for (int i = 1; i < *argc; i++) {
        if (strcmp(argv[i], "-MD") == 0)
            argv[new_argc++] = "--MD";
        else if (strcmp(argv[i], "-M") == 0)
            argv[new_argc++] = "--M";
        else
            argv[new_argc++] = argv[i];
    }
    *argc = new_argc;
}

int cli_setup_internal_libc(cli_options_t *opts, const char *prog)
{
    if (!opts->vc_sysinclude || !*opts->vc_sysinclude) {
        char tmp[PATH_MAX];
        snprintf(tmp, sizeof(tmp), "%s", prog);
        char *slash = strrchr(tmp, '/');
        if (slash)
            *slash = '\0';
        else
            strcpy(tmp, ".");
        size_t dirlen = strlen(tmp);
        if (dirlen + strlen("/libc/include") >= PATH_MAX) {
            fprintf(stderr, "Error: internal libc path too long.\n");
            return 1;
        }
        strcat(tmp, "/libc/include");
        opts->vc_sysinclude = vc_strdup(tmp);
        if (!opts->vc_sysinclude) {
            vc_oom();
            return 1;
        }
    }
    preproc_set_internal_libc_dir(opts->vc_sysinclude);

    const char *dir = opts->vc_sysinclude;
    char hdr[PATH_MAX];
    snprintf(hdr, sizeof(hdr), "%s/stdio.h", dir);
    if (access(hdr, F_OK) != 0) {
        fprintf(stderr, "Error: internal libc header '%s' not found.\n", hdr);
        return 1;
    }

    char libdir[PATH_MAX];
    snprintf(libdir, sizeof(libdir), "%s", dir);
    char *slash = strrchr(libdir, '/');
    if (slash)
        *slash = '\0';
    const char *libname = opts->use_x86_64 ? "libc64.a" : "libc32.a";
    char archive[PATH_MAX];
    if (snprintf(archive, sizeof(archive), "%s/%s", libdir, libname) >= (int)sizeof(archive)) {
        fprintf(stderr, "Error: internal libc archive path too long.\n");
        return 1;
    }
    if (access(archive, F_OK) != 0) {
        fprintf(stderr, "Error: internal libc archive '%s' not found.\n", archive);
        return 1;
    }
    return 0;
}
