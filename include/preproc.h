/*
 * Minimal source preprocessor.
 *
 * Supports '#include "file"' and '#include <file>', object-like '#define NAME value' and simple
 * single-parameter macros of the form '#define NAME(arg) expr'. Expansion is
 * purely textual and does not recognize strings or comments.  Basic
 * conditional directives ('#if', '#ifdef', '#ifndef', '#elif', '#else',
 * '#endif') are supported with simple expression evaluation.
 */

#ifndef VC_PREPROC_H
#define VC_PREPROC_H

#include "preproc_file.h"

#endif /* VC_PREPROC_H */
