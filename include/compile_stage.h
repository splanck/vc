/*
 * Compilation pipeline orchestrator.
 *
 * Provides a helper for running all compilation stages for a single
 * translation unit.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_COMPILE_STAGE_H
#define VC_COMPILE_STAGE_H

#include "cli.h"

/* Run the full compilation pipeline on SOURCE. */
int compile_pipeline(const char *source, const cli_options_t *cli,
                     const char *output, int compile_obj);

#endif /* VC_COMPILE_STAGE_H */
