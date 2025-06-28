# Variadic Functions

See the [documentation index](index.md) for a list of all available pages.

Functions can be declared with a trailing `...` in the parameter list to
accept a variable number of arguments. The extra arguments are pushed on
the stack and may be accessed using the standard `<stdarg.h>` macros.

Only integer and pointer arguments are currently reliable; passing
floating-point values as variadic parameters is not supported.

Function pointers can be declared using the familiar `(*name)(...)` syntax
and invoked just like normal function identifiers.

## Standard Preprocessor Macros

The built-in preprocessor defines four special identifiers that are replaced
without needing an explicit `#define`:

- `__FILE__` expands to the current source file name as a string literal.
- `__LINE__` expands to the current line number.
- `__DATE__` and `__TIME__` expand to the compilation date and time strings.
