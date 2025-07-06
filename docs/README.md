# vc Documentation Index

`vc` is a lightweight ANSI C compiler with experimental C99 support. This page
provides an overview of all available documentation.

## Documents

- [Compiler architecture](architecture.md) – overview of the compiler modules
  and their interaction.
- [Building vc](building.md) – requirements, build options and how to run the
  tests.
- [Compilation pipeline](pipeline.md) – detailed description of each stage of
  compilation.
- [Preprocessor workflow](preprocessor.md) – how macros and directives are
  handled before lexing.
- Built-in macros – `__FILE__`, `__LINE__`, `__DATE__`, `__TIME__`,
  `__STDC__`, `__STDC_VERSION__` and `__func__` expand automatically.
- [Supported language features](language_features.md) – syntax and semantics
  currently implemented, including variadic functions and struct return values.
- [Command-line usage](command_line.md) – available options and example
  invocations, including the `--intel-syntax` flag for Intel-style assembly.
- [Optimization passes](optimization.md) – constant folding, propagation and
  dead code elimination.
- [Development roadmap](roadmap.md) – planned milestones and compatibility
  goals.
- [Contributing](../CONTRIBUTING.md) – how to submit patches and bug reports.
