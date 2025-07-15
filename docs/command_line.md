## Command-line Options

See the [documentation index](README.md) for a list of all available pages.

This page describes the available flags and demonstrates how to compile a
simple program with **vc**.

## Table of Contents

- [Command-line Options](#command-line-options)
- [Preprocessor Usage](#preprocessor-usage)
- [Compiling a Simple Program](#compiling-a-simple-program)

The compiler supports the following options:

- `-o`, `--output <file>` – path for the generated assembly.
- `-h`, `--help` – display help text and exit.
- `-v`, `--version` – print version information and exit.
- `--no-fold` – disable constant folding.
- `--no-dce` – disable dead code elimination.
- `--no-cprop` – disable constant propagation.
- `--no-inline` – disable inline expansion of small functions.
- `--debug` – emit `.file` and `.loc` directives in the assembly output.
- `--emit-dwarf` – include DWARF line and symbol data in the output.
- `--no-color` – disable ANSI colors in diagnostics.
- `--no-warn-unreachable` – disable warnings for unreachable statements.
- `--x86-64` – generate 64‑bit x86 assembly.
- `--intel-syntax` – enable Intel-style x86 assembly output. When used
  together with `--compile` or `--link`, the assembler must be `nasm`.
- `-c`, `--compile` – assemble the output into an object file using `cc -c`.
- `--link` – build an executable by assembling and linking with `cc`.
- `--obj-dir <path>` – directory for temporary object files.
- `--sysroot=<dir>` – prepend `<dir>` to builtin include paths.
- `--vc-sysinclude=<dir>` – prepend `<dir>` to the system header list.
- `--internal-libc` – use the bundled libc headers and archive when linking.
- `--verbose-includes` – print each include directory searched and the
  final resolved path.
- `-S`, `--dump-asm` – print the generated assembly to stdout instead of creating a file.
- `--dump-ast` – print the AST to stdout after parsing.
- `--dump-ir` – print the IR to stdout before code generation.
- `--dump-tokens` – print the token list to stdout after preprocessing.
- `-M` – generate a `.d` file listing the source and headers.
- `-MD` – like `-M` but also compile the source.
- `--std=<c99|gnu99>` – select the language standard (default is `c99`).
- `-E`, `--preprocess` – print the preprocessed source to stdout and exit.
- `-I`, `--include <dir>` – add directory to the `#include` search path.
- `-L<dir>` – add a directory to the library search path when linking.
- `-l<name>` – link against the specified library.
- `-Dname[=val]` – define a preprocessor macro before compilation. When
  `val` contains spaces it may be quoted as in `-Dname="some value"`. Any
  surrounding single or double quotes are stripped.
- `-Uname` – undefine a macro before compilation.
- `-fmax-include-depth=<n>` – set the maximum nested `#include` depth.
- `-O<N>` – set optimization level (0 disables all passes).

The compiler warns about statements that cannot be reached because a
`return` or `goto` to the function's end label appears earlier. Use
`--no-warn-unreachable` to silence this warning.

Temporary object and assembly files are written to the directory given with
the `--obj-dir` option when provided.  Without this flag the compiler
consults the `TMPDIR` environment variable and then `P_tmpdir` when set.
Only if neither variable is available does it fall back to `/tmp`.
The assembler can be overridden with the `AS` environment variable while
`CC` specifies the linker command and the default assembler for AT&T
syntax.  When unset they default to `nasm` (Intel mode) and `cc`.
Additional options may be supplied in the `VCFLAGS` environment variable.
Its contents are split on spaces and prepended to the argument vector so
flags provided directly on the command line override those from
`VCFLAGS`. Values containing spaces may be quoted with single or double
quotes which will be removed during parsing.

Use `vc -o out.s source.c` to compile a file, `vc -c -o out.o source.c` to
produce an object, `vc --link -o prog main.c util.c` to build an executable
from multiple sources, `vc -S source.c` to print the assembly to the
terminal, or pipe code into `vc -o out.s -` to read from standard input.
When `-c` is combined with several inputs an object named after each
source (for example `file.o`) is created in the current directory.

## Preprocessor Usage

The preprocessor runs automatically before the lexer. It supports both
`#include "file"` and `#include <file>` to insert the contents of another
file. Additional directories to search for included files can be provided
with the `-I`/`--include` option. Angle‑bracket includes search those
directories, then any paths listed in the `VCPATH` environment variable
(colon separated, or semicolons on Windows), followed by the standard
system locations such as `/usr/include`. Quoted includes also consult
directories from `VCINC`. Entries from `VCPATH` and `VCINC` are appended
after all `-I` directories so command-line paths are searched first.
Directories from `VC_SYSINCLUDE` or the `--vc-sysinclude` option are
searched before the builtin list. When `--internal-libc` is used the
compiler automatically inserts the bundled `libc/include` directory using
an absolute path, so it works from any directory. When `--sysroot` is
used these builtin locations are prefixed with the provided directory.
For example:

```sh
VCPATH=/opt/headers VCINC=./quoted vc -I ./inc -o prog.s prog.c
```

The compiler will search `./inc` first, then `/opt/headers` for `<...>`
includes and `./quoted` for `"..."` includes before falling back to the
system directories.

object-like `#define` macros and parameterized
macros such as `#define NAME(a, b)`; macro bodies are expanded recursively.
The `#` operator stringizes a parameter and `##` concatenates two tokens during
expansion. Macros may be removed with `#undef NAME`.

Use the `-E`/`--preprocess` option to run just the preprocessor and print the
expanded source to the terminal.

```c
#define VAL 3
#define ADD(a, b) ((a) + (b))
#define DOUBLE(x) ADD(x, x)
#include "header.h"
int main() { return DOUBLE(VAL); }
```

Macros may be removed with `#undef` and redefined later:

```c
#define TEMP 1
#undef TEMP
#define TEMP 2
```

Conditional blocks may be controlled using the `defined` operator or
numeric expressions:

```c
#define FEATURE
#if defined(FEATURE)
int main() { return 1; }
#else
int main() { return 0; }
#endif
```

The preprocessor strips comments and detects string or character literals so
macros within them are not expanded. Conditional directives are honored and
support the `defined` operator along with numeric constants and the `!`, `&&`
and `||` operators.

The directive `#line <num> ["file"]` may be used to reset the reported line
number (and optionally source file) for subsequent tokens. The preprocessor
emits GCC-style markers such as `# 42 "path.c"` which are consumed by the
lexer and do not appear in the parsed token stream.

## Compiling a Simple Program

Example source files can be found under `tests/fixtures`. The simplest is
`simple_add.c`, which returns the result of `1 + 2 * 3`. Compile it with
optimizations disabled and request 64‑bit assembly like so:

```sh
vc -O0 --x86-64 -o simple_add.s tests/fixtures/simple_add.c
```

The generated file `simple_add.s` should match
`tests/fixtures/simple_add.s`:

```asm
main:
    pushl %ebp
    movl %esp, %ebp
    movl $7, %eax
    movl %eax, %eax
    ret
```

Any program compiled with the same options will produce assembly identical to
the corresponding `.s` file under `tests/fixtures`.

## Developer Notes

Command-line arguments are parsed in `cli_parse_args`. The function delegates
most work to helper routines:

- `load_vcflags` – prepends options from the `VCFLAGS` environment variable.
- `scan_shortcuts` – expands shorthand flags like `-M` and `-MD`.
- `parse_optimization_opts` – toggles optimization passes and sets `-O` levels.
- `parse_io_paths` – collects include directories, library paths and output
  locations.
- `parse_misc_opts` – processes all remaining flags such as `--debug` or
  `--link`.
- `finalize_options` – validates the parsed state and gathers source files.

Splitting the logic keeps `cli_parse_args` short and makes each option group
easier to maintain.
