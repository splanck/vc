#ifndef VC_CLI_OPTS_ENV_H
#define VC_CLI_OPTS_ENV_H
#include "cli.h"

int load_vcflags(int *argc, char ***argv, char ***out_argv, char **out_buf);
void scan_shortcuts(int *argc, char **argv);
int cli_setup_internal_libc(cli_options_t *opts, const char *prog);
#endif /* VC_CLI_OPTS_ENV_H */
