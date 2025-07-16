#include "compile_optimize.h"

int compile_optimize_impl(ir_builder_t *ir, const opt_config_t *cfg)
{
    if (cfg)
        opt_run(ir, cfg);
    return 1;
}

