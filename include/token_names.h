/*
 * Token name lookup helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_TOKEN_NAMES_H
#define VC_TOKEN_NAMES_H

#include "token.h"

/* Map a token type to a human readable name used in diagnostics */
const char *token_name(token_type_t type);

#endif /* VC_TOKEN_NAMES_H */
