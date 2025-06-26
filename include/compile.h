/*
 * Compilation helper interfaces.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_COMPILE_H
#define VC_COMPILE_H

#include "cli.h"

/* Compile a single translation unit. */
int compile_unit(const char *source, const cli_options_t *cli,
                 const char *output, int compile_obj);

/* Create object file with program entry point for linking. */
int create_startup_object(int use_x86_64, char **out_path);

#endif /* VC_COMPILE_H */
