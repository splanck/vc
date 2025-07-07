/*
 * High level compilation and linking helpers.
 *
 * Provides routines for running the preprocessor, compiling
 * translation units and linking object files.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_COMPILE_H
#define VC_COMPILE_H

#include "cli.h"
#include "token.h"

/* Compile a single translation unit. */
int compile_unit(const char *source, const cli_options_t *cli,
                 const char *output, int compile_obj);

/* Convert a token array into a printable string. Caller frees result. */
char *tokens_to_string(const token_t *toks, size_t count);

/* Run only the preprocessor stage and print the result. */
int run_preprocessor(const cli_options_t *cli);

/* Generate dependency files without compiling */
int generate_dependencies(const cli_options_t *cli);

/* Compile multiple sources and link them into an executable. */
int link_sources(const cli_options_t *cli);

#endif /* VC_COMPILE_H */
