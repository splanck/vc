# vc Internal libc

This directory contains a minimal C standard library used by the compiler's
integration tests and small example programs. It currently targets two
architectures:

- **i386** following the ILP32 model
- **x86‑64** following the LP64 model

Only a few functions are implemented:

- `puts` and a very small `printf`
- `exit`, `malloc` and `free`
- `strlen`, `memcpy` and `memmove`

Other standard interfaces are intentionally omitted for now. Programs that
require features like file I/O must be built against the host system libc
instead of the bundled one.

## Building

Run the following commands from the repository root:

```sh
make libc32    # build libc/libc32.a for 32-bit code
make libc64    # build libc/libc64.a for 64-bit code
make all       # build both archives alongside the compiler
```

## Extending the library

Public headers live in `libc/include` and must use traditional include guards.
Implementations belong in `libc/src`. When adding new files update
`libc/Makefile` so they are compiled for both bit widths. Keep the
implementations simple and portable.
