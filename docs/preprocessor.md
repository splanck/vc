# Preprocessor Workflow

The preprocessor is the first stage of the compilation pipeline.  It reads
source files, expands macros and resolves conditional blocks before any tokens
are produced.  `preproc_run` in `src/preproc_file.c` drives this process.

## File processing

`preproc_run` builds a list of include search paths then calls `process_file` to
read the initial source.  The list is released with [`free_string_vector`](memory_helpers.md) once preprocessing finishes.  Each line is inspected in order and any recognised
preprocessor directive is handled immediately:

- `#include` resolves the requested path and recursively invokes `process_file`
  on the included file when the current conditional state is active.
- `#include_next` continues the search from the directory following the one
  that contained the current file.
- `#define` adds a new macro definition.  Parameterised forms are supported and
  stored in a `macro_t` structure.  Lines ending in `\` are joined so a macro
  body may span multiple lines.
- `#undef` removes existing definitions.
- `#error` prints the given message to stderr and aborts preprocessing when
  encountered in an active block.
- `#warning` prints the given message to stderr but continues preprocessing
  when encountered in an active block.
- Conditional directives (`#if`, `#ifdef`, `#ifndef`, `#elif`, `#else` and
  `#endif`) manipulate a stack of state objects so nested conditions may be
  evaluated correctly.
- `#pragma` directives are ignored unless recognised. Supported forms are
  `#pragma pack(push,n)` which updates the current struct packing alignment and
  `#pragma pack(pop)` which restores the previous value.
- `#pragma once` marks the current file so subsequent includes of the same
  path are ignored.
- Any other line has macros expanded and is appended to the output buffer.

A simple usage example is:

```c
#warning "incomplete feature"
```

At the end of processing the combined text is returned to the compiler and fed
into the lexer.

## Macro handling

Macros are stored in a simple vector declared in `preproc_macros.h`.  Each
`macro_t` holds the macro name, an optional parameter list and its body text.
`expand_line` loops over each character delegating to `parse_macro_invocation`
for identifiers or `emit_plain_char` otherwise.  The invocation helper parses
any argument list and calls `expand_macro_call` so expansion remains recursive.
`expand_params` continues to rely on helper routines that perform parameter
lookup, handle the `#` stringize operator and manage `##` token pasting.  A
macro may be declared variadic by using `...` as the final parameter.  When such
a macro is invoked `__VA_ARGS__` within its body is replaced by the remaining
arguments.
The macro table is cleaned up with [`free_macro_vector`](memory_helpers.md) once preprocessing is complete.
Macro expansion is recursive so macro bodies may reference other macros. To
avoid infinite loops a hard limit of 4096 nested expansions is enforced.  When
this limit is hit `expand_line` returns zero and the compiler aborts
preprocessing after printing "Macro expansion limit exceeded".

File inclusion works the same way and may recurse when headers themselves
contain `#include` directives.  To guard against unbounded recursion the
preprocessor enforces a maximum include depth of 20 files.  If this limit is
exceeded the build stops with an "Include depth limit exceeded" error.

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
- `__BASE_FILE__` holds the name of the initial source file being
  processed.
- `__COUNTER__` expands to an incrementing integer starting at `0`.

These macros are always available and cannot be undefined. They are useful for
diagnostics and logging as they convey file names, line numbers and build
timestamps.

Additional macros may be defined on the command line using `-Dname=value` or in
source files with `#define`. After preprocessing the expanded text is handed to
the lexer for tokenization.
## Preprocessor context

`preproc_context_t` is defined in `include/preproc_file.h` and is passed to `preproc_run`. It contains several fields:

```c
vector_t pragma_once_files; /* headers marked with #pragma once */
vector_t deps;              /* all processed files */
vector_t pack_stack;        /* active #pragma pack values */
size_t pack_alignment;      /* current packing alignment */
int in_comment;             /* multi-line comment state */
```

`pragma_once_files` tracks headers that issued `#pragma once` so they are not
processed again. `deps` records every file read during preprocessing, enabling
dependency generation. Because the entire context is provided by the caller
rather than stored globally, multiple preprocessing operations can run
independently. Each call initializes and cleans up the vectors, allowing the
preprocessor to be used reentrantly by supplying a separate
`preproc_context_t` instance.

