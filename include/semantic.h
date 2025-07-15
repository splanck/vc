/*
 * Aggregate semantic analysis interface.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_H
#define VC_SEMANTIC_H

#include "consteval.h"
#include "semantic_expr.h"
#include "semantic_stmt.h"
#include "semantic_decl_stmt.h"
#include "semantic_loops.h"
#include "semantic_control.h"
#include "semantic_global.h"
#include "semantic_inline.h"

void semantic_set_x86_64(int flag);
int semantic_get_x86_64(void);

#endif /* VC_SEMANTIC_H */
