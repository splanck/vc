#ifndef VC_OPT_H
#define VC_OPT_H

#include "ir.h"

typedef struct {
    int fold_constants; /* enable constant folding */
    int dead_code;      /* enable dead code elimination */
    int const_prop;     /* enable store/load constant propagation */
} opt_config_t;

/* Run optimization passes on the given IR builder */
void opt_run(ir_builder_t *ir, const opt_config_t *cfg);

#endif /* VC_OPT_H */
