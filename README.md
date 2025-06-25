# vc

`vc` is a lightweight ANSI C compiler with experimental C99 support. It
aims to be portable across POSIX systems with a focus on NetBSD.

- [Compiler architecture](docs/architecture.md)
- [Building vc](docs/building.md)
- [User documentation](docs/vcdoc.md)
- [Contributing](CONTRIBUTING.md)

Development is in an early stage. See the documents above for more
information on how the pieces fit together and how to get involved.

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

## Installation

To install the compiler, run:

```sh
make install PREFIX=/usr/local
```

This installs `vc` under `PREFIX/bin`, the public headers under
`PREFIX/include/vc`, and the manual page under `PREFIX/share/man/man1`.
