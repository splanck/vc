# vc

## Overview

`vc` is a lightweight ANSI C compiler with experimental C99 support. The
project aims to be portable across POSIX systems with a focus on NetBSD
while remaining small and easy to understand. Development is in an
early stage with the long‑term goal of eventually becoming
self‑hosting.

## Installation

To install the compiler, run:

```sh
make install PREFIX=/usr/local
```

This installs `vc` under `PREFIX/bin`, the public headers under
`PREFIX/include/vc`, and the manual page under `PREFIX/share/man/man1`.

## Usage

Compile a source file to assembly:

```sh
vc -o out.s source.c
```

To generate an object file instead, pass `-c`:

```sh
vc -c -o out.o source.c
```

When `-c` is given more than once source file may be listed.  An object
file named after each input (e.g. `foo.o`) will be produced in the
current directory.

To print the generated assembly to stdout instead of creating a file,
pass `--dump-asm`:

```sh
vc --dump-asm source.c
```

Enable debug directives with `--debug`:

```sh
vc --debug -S source.c
```

Disable colored diagnostics with `--no-color` when capturing output.

The compiler also consults a few environment variables for additional include
search paths. Any directories listed in `VCPATH`, `VCINC`, `CPATH` or
`C_INCLUDE_PATH` are appended after `-I` options and searched before the builtin
locations.

## Examples

Several small example programs live in the `examples` directory. They
illustrate how to build and run code with `vc`. See
[examples/README.md](examples/README.md) for full details.

## Testing

The project ships with a small unit and integration test suite. A C99
compiler such as `gcc` and a `make` implementation are required. Run the
tests from the repository root with one of the following commands:

```sh
make test            # builds vc and runs tests/run_tests.sh
tests/run.sh         # builds vc, compiles unit tests and runs everything
```

Both scripts exit with status 0 when all checks pass. The output ends with
`All tests passed` on success.

The suite includes an optional check using glibc's `<sys/cdefs.h>` to
exercise `_Pragma` handling. If this header cannot be located or fails to
preprocess, the check is skipped and the remaining tests still run normally.

## Documentation

The [documentation index](docs/README.md) provides an overview of all available
pages. Key documents include:

- [Compiler architecture](docs/architecture.md)
- [Compilation pipeline](docs/pipeline.md)
- [Command-line options](docs/command_line.md)
- [Supported language features](docs/language_features.md)
- [Optimization passes](docs/optimization.md)
- [Building vc](docs/building.md)
- [Development roadmap](docs/roadmap.md)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.
