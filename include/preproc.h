/*
 * Minimal source preprocessor.
 *
 * Supports '#include "file"' and '#include <file>', object-like '#define NAME value' and simple
 * single-parameter macros of the form '#define NAME(arg) expr'. Comments are
 * stripped and string/character literals are detected so macros inside them are
 * not expanded.  Basic
 * conditional directives ('#if', '#ifdef', '#ifndef', '#elif', '#else',
 * '#endif') are supported with simple expression evaluation.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PREPROC_H
#define VC_PREPROC_H

#include "preproc_file.h"
#include "preproc_path.h"
#include "preproc_cond.h"

#endif /* VC_PREPROC_H */
