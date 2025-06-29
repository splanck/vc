# Preprocessor Workflow

The preprocessor is the first stage of the compilation pipeline.  It reads
source files, expands macros and resolves conditional blocks before any tokens
are produced.  `preproc_run` in `src/preproc_file.c` drives this process.

## File processing

`preproc_run` builds a list of include search paths then calls `process_file` to
read the initial source.  Each line is inspected in order and any recognised
preprocessor directive is handled immediately:

- `#include` resolves the requested path and recursively invokes `process_file`
  on the included file when the current conditional state is active.
- `#define` adds a new macro definition.  Parameterised forms are supported and
  stored in a `macro_t` structure.
- `#undef` removes existing definitions.
- `#error` prints the given message to stderr and aborts preprocessing when
  encountered in an active block.
- Conditional directives (`#if`, `#ifdef`, `#ifndef`, `#elif`, `#else` and
  `#endif`) manipulate a stack of state objects so nested conditions may be
  evaluated correctly.
- `#pragma` lines are passed through verbatim when active.
- Any other line has macros expanded and is appended to the output buffer.

At the end of processing the combined text is returned to the compiler and fed
into the lexer.

## Macro handling

Macros are stored in a simple vector declared in `preproc_macros.h`.  Each
`macro_t` holds the macro name, an optional parameter list and its body text.
`expand_line` loops over each character delegating to `parse_macro_invocation`
for identifiers or `emit_plain_char` otherwise.  The invocation helper parses
any argument list and calls `expand_macro_call` so expansion remains recursive.
`expand_params` continues to rely on helper routines that perform parameter
lookup, handle the `#` stringize operator and manage `##` token pasting.
Macro expansion is recursive so macro bodies may reference other macros. To
avoid infinite loops a hard limit of 100 nested expansions is enforced. The
compiler aborts preprocessing with an error message as soon as this depth is
reached.

Conditional expressions in `#if` directives are parsed by the small recursive
descent parser in `preproc_expr.c`.  The `defined` operator queries the current
macro table so feature tests work as expected.

## Standard macros

Several identifiers are predefined by the compiler and expand without needing
an explicit `#define`:

- `__FILE__` expands to the current source file name as a string literal.
- `__LINE__` expands to the current line number.
- `__DATE__` and `__TIME__` expand to the compilation date and time strings.
- `__STDC__` evaluates to `1` to indicate standard compliance.
- `__STDC_VERSION__` expands to `199901L` for C99 support.
- `__func__` yields the enclosing function name as a string literal.
