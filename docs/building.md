# Building vc

See the [documentation index](README.md) for a list of all available pages.

## Table of Contents

- [Requirements](#requirements)
- [Build options](#build-options)
- [Bundled libc](#bundled-libc)
- [Additional build steps](#additional-build-steps)
- [Running the test suite](#running-the-test-suite)
- [Builtin preprocessor macros](#builtin-preprocessor-macros)

`vc` targets POSIX systems with a focus on NetBSD. Building on other BSD
variants such as FreeBSD or OpenBSD should work with little or no
modification.

## Requirements

- Standard C tool chain capable of C99
- `make` (on NetBSD) or `gmake` on systems where GNU make is not the
  default

## Build options

The default build uses NetBSD specific settings. When building on other
BSDs, set the `PLATFORM` variable to `generic`:

```sh
make PLATFORM=generic
```

This disables any NetBSD specific extensions.

On Linux the compiler also searches `/usr/include/<multiarch>` for headers.
The multiarch directory is detected at runtime using `gcc -print-multiarch`
(via the `gcc` in `PATH`). If detection fails the generic directory
`x86_64-linux-gnu` is used.  The detected value can be overridden at build time
with the `MULTIARCH` variable if needed:

```sh
make MULTIARCH=arm-linux-gnueabihf
```

The GCC internal include directory reported by `cc -print-file-name=include`
is also added so headers like `stddef.h` resolve correctly. The Makefile
removes the trailing newline from this path using `tr -d '\n'` to ensure
`src/preproc_path.c` receives a clean directory string without whitespace.

`vc` can generate either 32-bit or 64-bit x86 assembly. Use the
`--x86-64` flag when invoking the compiler to enable 64-bit output. The
default without this flag is 32-bit code.

To dump the generated assembly to stdout instead of creating a file, use
`--dump-asm`. When this flag is given the `-o` option is not required.

To assemble the output directly into an object file, pass `-c` or
`--compile` along with an output path ending in `.o`. The compiler will
invoke `cc -c` on the generated assembly to produce the object file.
When the `--intel-syntax` flag is used together with `--compile` or the
`--link` option, the build requires `nasm` to assemble the Intel-style
output.

## Bundled libc

The repository includes a small C library under `libc` used for tests and
examples. Running `make` builds the compiler and both `libc/libc32.a` and
`libc/libc64.a`. They may also be built individually:

```sh
make libc32
make libc64
```

Invoke the compiler with `--internal-libc` to search the bundled
`libc/include` directory before system paths and link the matching
archive regardless of the current working directory. Additional system
header locations can be supplied with `--vc-sysinclude=<dir>` or the
`VC_SYSINCLUDE` environment variable.

When combined with `--x86-64` the compiler now evaluates constant
expressions and orders function call arguments assuming 64-bit pointers.
This ensures programs linked against the bundled libc behave correctly
on LP64 systems.

If the selected archive is missing the compiler prints an error similar to
`vc: internal libc archive 'libc/libc64.a' not found. Build it with 'make libc64'`.

## Additional build steps

Extra source files can be passed to the build using the `EXTRA_SRC`
variable. Compiler optimization levels may be controlled with
`OPTFLAGS`:

```sh
make EXTRA_SRC="src/utils.c src/driver.c" OPTFLAGS="-O2"
```

See the [README](../README.md) for an overview of the project.

## Running the test suite

The project includes a small set of unit and integration tests. They can be
executed from the repository root with:

```sh
tests/run.sh
```

This script builds the compiler, compiles the unit test harness for the lexer
and parser, and then runs both the unit tests and the integration tests found
under `tests/`. It returns a non-zero status if any test fails.

## Builtin preprocessor macros

Several macros expected by system headers are defined automatically during
compilation. Most values mirror the output of `$(CC) -dM -E` so the
preprocessor behaves like GCC.  The compiler also provides a set of standard
definitions:

- `__STDC__` and `__STDC_HOSTED__` evaluate to `1`.
- `__i386__` or `__x86_64__` is defined based on the selected bit width.
- `__SIZE_TYPE__` and `__PTRDIFF_TYPE__` use `unsigned int`/`int` on ILP32 and
  `unsigned long`/`long` on LP64.
- `__GNUC__`, `__GNUC_MINOR__`, `__GNUC_PATCHLEVEL__`
- `__WCHAR_TYPE__`, `__WINT_TYPE__`, `__INTMAX_TYPE__`, `__UINTMAX_TYPE__`,
  `__INTPTR_TYPE__`, `__UINTPTR_TYPE__`

These defaults prevent runaway expansions when processing standard library
headers.

## Improved diagnostics

Error messages produced during compilation now include the source file,
line and column, and when applicable the current function name.  The
format is:

```
file:line:column: function: message
```

If an error occurs outside a function, the function portion is omitted.

Macro expansions now track the column where each invocation begins.  When
an error originates from within a macro body or uses `__FILE__`/`__LINE__`,
the reported position refers to the invocation site rather than the macro
definition.
