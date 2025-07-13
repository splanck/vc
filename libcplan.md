# Plan for integrating a minimal internal libc into vc

## Overview
This plan outlines the steps required to add an internal C standard library implementation to the `vc` compiler.  The aim is to provide a small set of headers and implementations that can be used instead of the host system libc when requested.  When enabled, `vc` should search its internal headers before system paths and link against a static archive `libc.a` built from the new sources.

## Directory structure
1. Create a new top level directory `libc/`.
2. Inside `libc/` add two subdirectories:
   - `include/` – public headers (`stdio.h`, `stdlib.h`, `string.h` and private helpers).
   - `src/` – implementation files.
3. Add a build fragment `libc/Makefile` (or `CMakeLists.txt` if CMake is adopted later).  It will compile every `*.c` file in `libc/src` with `-Ilibc/include` and archive the objects into `libc/libc.a`.

## Minimal headers
Place the following headers under `libc/include` each with traditional include guards:
- **stdio.h** – declarations for `int puts(const char *);` and `int printf(const char *, ...);`.
- **stdlib.h** – declarations for `void exit(int);`, `void *malloc(unsigned long);` and `void free(void *);`.
- **string.h** – declarations for `unsigned long strlen(const char *);` and `void *memcpy(void *, const void *, unsigned long);`.
- **_vc_syscalls.h** – private header with forward declarations of low level I/O or memory helpers that the libc implementation may use later.

## Source stubs
Create matching implementation files under `libc/src`:
- **stdio.c** – provide trivial implementations of `puts` and `printf`.  Initially they may call the host `write` syscall or `fputs` to `stdout` through helpers defined in `_vc_syscalls.h`.
- **stdlib.c** – implement `exit`, `malloc` and `free` by delegating to the host routines or by simple wrappers.
- **string.c** – implement naive but correct versions of `strlen` and `memcpy`.

## Build integration
1. Extend the main project `Makefile` so running `make all` also builds `libc/libc.a`.
2. Include variables `LIBC_SRC` and `LIBC_OBJ` for the new sources and add rules to compile them with the header search path set to `libc/include`.
3. Add a `clean` target that removes the objects and archive.

## Compiler driver changes
1. **Header search paths**
   - Introduce an environment variable `VC_SYSINCLUDE` and a command line option `--vc-sysinclude=<dir>` (short form `-isysroot`).
   - Update `collect_include_dirs()` in `src/preproc_path.c` to insert this directory (or directories) at the start of the builtin system include list when supplied.
   - When the flag is used it should cause angle bracket includes to be resolved against the specified directory before the default `/usr/include` locations.

2. **Internal libc flag**
   - Add a new CLI option `--internal-libc` that enables use of the bundled libc.  This sets a flag in `cli_options_t`.
   - When this flag is active, `collect_include_dirs()` automatically prepends `$(PROJECT_ROOT)/libc/include` to the system search list.
   - Modify `build_and_link_objects()` in `src/compile_link.c` so that, unless `-nostdlib` is given, `libc/libc.a` is linked after user objects when `--internal-libc` is set.

3. **Built‑in macros**
   - Ensure `define_default_macros()` defines essential macros such as `__STDC__`, `__x86_64__`, and `__SIZE_TYPE__` when the host compiler does not provide them.  Document these defaults in `docs/building.md`.

4. **Diagnostics**
   - Add verbose mode output in `find_include_path()` that prints each directory searched and the file chosen when resolving `#include <...>` directives.

5. **Error handling**
   - During startup, if `--internal-libc` is used but `libc/include/stdio.h` cannot be found, print a clear error suggesting to build the libc or adjust `--vc-sysinclude`.

## Tests
1. Add a new fixture `tests/fixtures/libc_puts.c` that includes `<stdio.h>` and calls `puts("hello")`.
2. Extend `tests/run_tests.sh` to compile and link this program with `--internal-libc` and verify it runs successfully.

## Documentation
1. Create `libc/README.md` describing the purpose of the internal libc, current functionality, and how to rebuild it (`make libc` or as part of `make all`).
2. Add `libc/TODO` listing future headers to implement (e.g. `ctype.h`, `errno.h`, `stdint.h`), roughly ordered by how often they are likely to be needed.
3. Document the new command line options in `docs/command_line.md` and update `docs/building.md` with instructions on using the internal libc.

## Continuous integration
Update `.github/workflows/ci.yml` to build `libc/libc.a` and run the new libc integration test as part of the workflow.  This ensures pull requests cannot merge unless the internal headers resolve correctly and the bundled libc links.

## Summary of workflow
- Developers build the project normally with `make` which now also builds `libc/libc.a`.
- To use the bundled libc, invoke the compiler with `--internal-libc` or set `VC_SYSINCLUDE=libc/include`.  The driver then searches these headers first and links against the archive automatically.
- Passing `-nostdlib` disables linking with the internal library for freestanding or system builds.

This plan establishes a minimal but functional libc within the repository and integrates it with the existing build and test infrastructure.
