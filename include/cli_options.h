#ifndef VC_CLI_OPTIONS_H
#define VC_CLI_OPTIONS_H

#include "cli.h"

int handle_help(const char *arg, const char *prog, cli_options_t *opts);
int handle_version(const char *arg, const char *prog, cli_options_t *opts);
int add_define_opt(const char *arg, const char *prog, cli_options_t *opts);
int add_undef_opt(const char *arg, const char *prog, cli_options_t *opts);
int parse_optimization_opts(int opt, const char *arg, cli_options_t *opts);
int parse_io_paths(int opt, const char *arg, cli_options_t *opts);
int parse_misc_opts(int opt, const char *arg, const char *prog,
                    cli_options_t *opts);

#endif /* VC_CLI_OPTIONS_H */
