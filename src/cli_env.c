#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "cli_env.h"
#include "util.h"

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

        size_t new_count = (size_t)*argc + vcargc + 1;
        if (new_count > SIZE_MAX / sizeof(char *)) {
            fprintf(stderr, "vc: argument vector too large\n");
            free(vcbuf);
            return 1;
        }

        size_t alloc_sz = sizeof(char *) * new_count;
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

        vcargv[idx] = NULL;

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
