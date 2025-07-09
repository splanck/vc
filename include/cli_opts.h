#ifndef VC_CLI_OPTS_H
#define VC_CLI_OPTS_H
#include "cli.h"

void print_usage(const char *prog);
int parse_optimization_opts(int opt, const char *arg, cli_options_t *opts);
int parse_io_paths(int opt, const char *arg, cli_options_t *opts);
int parse_misc_opts(int opt, const char *arg, const char *prog, cli_options_t *opts);
int finalize_options(int argc, char **argv, const char *prog, cli_options_t *opts);

#endif /* VC_CLI_OPTS_H */
