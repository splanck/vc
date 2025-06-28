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

To print the generated assembly to stdout instead of creating a file,
pass `--dump-asm`:

```sh
vc --dump-asm source.c
```

Enable debug directives with `--debug`:

```sh
vc --debug -S source.c
```

## Documentation

The [documentation index](docs/index.md) provides an overview of all available
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
