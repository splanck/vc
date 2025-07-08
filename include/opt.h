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
    int inline_funcs;   /* inline small functions */
} opt_config_t;

/* Print an optimization error message */
void opt_error(const char *msg);

/* Assign alias sets to memory operations */
void compute_alias_sets(ir_builder_t *ir);

/*
 * Run optimization passes on the given IR builder.
 *
 * Passes execute in the following order:
 * 1. Alias analysis
 * 2. Constant propagation
 * 3. Common subexpression elimination
 * 4. Inline expansion
 * 5. Constant folding
 * 6. Unreachable block elimination
 * 7. Dead code elimination
 */
void opt_run(ir_builder_t *ir, const opt_config_t *cfg);

#endif /* VC_OPT_H */
