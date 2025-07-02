/*
 * Startup stub helpers.
 *
 * Provides helpers for generating the program entry stub used during
 * linking.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_STARTUP_H
#define VC_STARTUP_H

#include "cli.h"

int write_startup_asm(int use_x86_64, asm_syntax_t syntax,
                      const cli_options_t *cli, char **out_path);
int assemble_startup_obj(const char *asm_path, int use_x86_64,
                         const cli_options_t *cli, char **out_path);

#endif /* VC_STARTUP_H */
