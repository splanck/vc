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
`expand_line` walks each line character by character replacing identifiers with
matching macros.  When a macro takes parameters the argument list is parsed and
substituted using `expand_params` which implements the `#` and `##` operators.
Macro expansion is recursive so macro bodies may reference other macros.

Conditional expressions in `#if` directives are parsed by the small recursive
descent parser in `preproc_expr.c`.  The `defined` operator queries the current
macro table so feature tests work as expected.
