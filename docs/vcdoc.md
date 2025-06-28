# vc Documentation

This page lists miscellaneous notes and examples for the compiler.

## Built-in macros

The preprocessor recognises several identifiers automatically:

- `__FILE__`, `__LINE__`, `__DATE__` and `__TIME__` expand to contextual information.
- `__STDC__` evaluates to `1`.
- `__STDC_VERSION__` expands to `199901L`.
- `__func__` expands to the current function name.

Refer to [preprocessor workflow](preprocessor.md) for further details.

## Variadic functions

Variadic functions now accept `float`, `double` and `long double` arguments in addition to integers and pointers.
