# Building vc

See the [documentation index](README.md) for a list of all available pages.

## Table of Contents

- [Requirements](#requirements)
- [Build options](#build-options)
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
The multiarch directory is determined at build time using `cc -print-multiarch`
and defaults to `x86_64-linux-gnu` when detection fails. The GCC internal
include directory reported by `cc -print-file-name=include` is also added so
headers like `stddef.h` resolve correctly. The Makefile removes the trailing
newline from this path using `tr -d '\n'` to ensure `src/preproc_path.c`
receives a clean directory string without whitespace.

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

Several macros commonly provided by GCC are defined automatically during
compilation. Their values are taken from the output of `$(CC) -dM -E` so the
preprocessor sees the same definitions as system headers. This includes:

- `__GNUC__`, `__GNUC_MINOR__`, `__GNUC_PATCHLEVEL__`
- `__SIZE_TYPE__`, `__PTRDIFF_TYPE__`, `__WCHAR_TYPE__`, `__WINT_TYPE__`
- `__INTMAX_TYPE__`, `__UINTMAX_TYPE__`, `__INTPTR_TYPE__`, `__UINTPTR_TYPE__`

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
