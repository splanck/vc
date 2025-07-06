# vc Documentation Index

`vc` is a lightweight ANSI C compiler with experimental C99 support. This page
provides an overview of the available documentation.

## Table of Contents

1. [Building vc](building.md) – requirements, build options and how to run the
   tests.
2. [Command-line usage](command_line.md) – available options and examples.
3. [Preprocessor workflow](preprocessor.md) – how macros and directives are
   handled before lexing. The standard macros `__FILE__`, `__LINE__`,
   `__DATE__`, `__TIME__`, `__STDC__`, `__STDC_VERSION__` and `__func__` are
   documented there.
4. [Compiler architecture](architecture.md) – overview of the compiler modules
   and their interaction.
5. [Compilation pipeline](pipeline.md) – detailed description of each stage of
   compilation.
6. [Optimization passes](optimization.md) – constant folding, propagation and
   dead code elimination.
7. [Supported language features](language_features.md) – syntax and semantics
   currently implemented.
8. [Development roadmap](roadmap.md) – planned milestones and compatibility
   goals.
9. [Contributing](../CONTRIBUTING.md) – how to submit patches and bug reports.
10. [Memory helpers](memory_helpers.md) – cleaning up vectors of strings or macros.
