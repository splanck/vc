/*
 * Optimization passes for IR.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include "opt.h"

/* Pass implementations */
void propagate_load_consts(ir_builder_t *ir);
void common_subexpr_elim(ir_builder_t *ir);
void inline_small_funcs(ir_builder_t *ir);
void fold_constants(ir_builder_t *ir);
void remove_unreachable_blocks(ir_builder_t *ir);
void dead_code_elim(ir_builder_t *ir);
void compute_alias_sets(ir_builder_t *ir);

/* Print an optimization error message */
void opt_error(const char *msg)
{
    fprintf(stderr, "optimizer: %s\n", msg);
}

/* Run enabled optimization passes on the IR */
void opt_run(ir_builder_t *ir, const opt_config_t *cfg)
{
    opt_config_t def = {1, 1, 1, 1, 1};
    const opt_config_t *c = cfg ? cfg : &def;
    compute_alias_sets(ir);
    if (c->const_prop)
        propagate_load_consts(ir);
    common_subexpr_elim(ir);
    if (c->inline_funcs)
        inline_small_funcs(ir);
    if (c->fold_constants)
        fold_constants(ir);
    remove_unreachable_blocks(ir);
    if (c->dead_code)
        dead_code_elim(ir);
}

