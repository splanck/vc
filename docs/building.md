# Building vc

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

## Additional build steps

Extra source files can be passed to the build using the `EXTRA_SRC`
variable. Compiler optimization levels may be controlled with
`OPTFLAGS`:

```sh
make EXTRA_SRC="src/utils.c src/driver.c" OPTFLAGS="-O2"
```

See the [README](../README.md) for an overview of the project.
