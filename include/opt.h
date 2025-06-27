/*
 * Optimization pass configuration.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_OPT_H
#define VC_OPT_H

#include "ir_core.h"

typedef struct {
    int opt_level;     /* numeric optimization level */
    int fold_constants; /* enable constant folding */
    int dead_code;      /* enable dead code elimination */
    int const_prop;     /* enable store/load constant propagation */
} opt_config_t;

/* Print an optimization error message */
void opt_error(const char *msg);

/*
 * Run optimization passes on the given IR builder.
 *
 * Passes execute in the following order:
 * 1. Constant propagation
 * 2. Constant folding
 * 3. Dead code elimination
 */
void opt_run(ir_builder_t *ir, const opt_config_t *cfg);

#endif /* VC_OPT_H */
