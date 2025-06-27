## Command-line Options

The compiler supports the following options:

- `-o`, `--output <file>` – path for the generated assembly.
- `-h`, `--help` – display help text and exit.
- `-v`, `--version` – print version information and exit.
- `--no-fold` – disable constant folding.
- `--no-dce` – disable dead code elimination.
- `--no-cprop` – disable constant propagation.
- `--x86-64` – generate 64‑bit x86 assembly.
- `-c`, `--compile` – assemble the output into an object file using `cc -c`.
- `--link` – build an executable by assembling and linking with `cc`.
- `-S`, `--dump-asm` – print the generated assembly to stdout instead of creating a file.
- `--dump-ir` – print the IR to stdout before code generation.
- `--std=<c99|gnu99>` – select the language standard (default is `c99`).
- `-E`, `--preprocess` – print the preprocessed source to stdout and exit.
- `-I`, `--include <dir>` – add directory to the `#include` search path.
- `-O<N>` – set optimization level (0 disables all passes).

Use `vc -o out.s source.c` to compile a file, `vc -c -o out.o source.c` to
produce an object, `vc --link -o prog main.c util.c` to build an executable
from multiple sources, or `vc -S source.c` to print the assembly to the
terminal.

## Preprocessor Usage

The preprocessor runs automatically before the lexer. It supports both
`#include "file"` and `#include <file>` to insert the contents of another
file. Additional directories to search for included files can be provided with
the `-I`/`--include` option. Angle-bracket includes search those directories,
then any paths listed in the `VCPATH` environment variable (colon separated),
followed by the standard system locations such as `/usr/include`. It also supports
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

Macro expansion is purely textual; macros inside strings or comments are not
recognized. Conditional directives are honored and support the `defined`
operator along with numeric constants and the `!`, `&&` and `||` operators.

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
