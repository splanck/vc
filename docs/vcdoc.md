# vc Documentation

This document describes the compilation pipeline, the role of each module
and the language features currently supported.

## Pipeline Overview

`vc` processes source code through several stages:

1. **Lexer** – converts the input into a stream of tokens.
2. **Parser** – builds an abstract syntax tree from the tokens.
3. **Semantic analyzer** – checks the AST and produces an intermediate representation (IR).
4. **Optimizer** – performs optional transformations on the IR.
5. **Register allocator** – assigns machine registers.
6. **Code generator** – emits target assembly.

The modules described below implement these steps.

## Modules

### lexer
Translates raw characters into tokens for the parser.

### parser
Constructs the AST and reports syntax errors.

### semantic
Performs type checking and converts the AST into IR.

### ir
Defines the IR structures used throughout the rest of the compiler.

### opt
Holds optimization passes such as constant folding and dead code elimination.

### regalloc
Handles register allocation for the backend.

### codegen
Emits assembly from the IR. Currently only x86 is supported.

## Supported Language Features

- Basic arithmetic expressions
- Function definitions and calls
- `for` and `while` loops
- Pointers

Examples below show how to compile each feature.

### Basic arithmetic
```c
/* add.c */
int main() {
    return 1 + 2 * 3;
}
```
Compile with:
```sh
vc -o add.s add.c
```

### Function calls
```c
/* call.c */
int mul(int a, int b) {
    return a * b;
}
int main() {
    return mul(2, 3);
}
```
Compile with:
```sh
vc -o call.s call.c
```

### Loops
```c
/* loop.c */
int main() {
    int i = 0;
    while (i < 5)
        i = i + 1;
    return i;
}
```
Compile with:
```sh
vc -o loop.s loop.c
```

### Pointers
```c
/* ptr.c */
int main() {
    int x = 42;
    int *p = &x;
    return *p;
}
```
Compile with:
```sh
vc -o ptr.s ptr.c
```

## Command-line Options

The compiler supports the following options:

- `-o`, `--output <file>` – path for the generated assembly.
- `-h`, `--help` – display help text and exit.
- `-v`, `--version` – print version information and exit.
- `--no-fold` – disable constant folding.
- `--no-dce` – disable dead code elimination.
- `--x86-64` – generate 64‑bit x86 assembly.
- `--dump-ir` – print the generated assembly to stdout instead of creating a file.

Use `vc -o out.s source.c` to compile a file, or `vc --dump-ir source.c` to
print the output to the terminal.

## Compiling a Simple Program

Example source files can be found under `tests/fixtures`. The simplest is
`simple_add.c`, which returns the result of `1 + 2 * 3`. Compile it with
optimizations disabled and request 64‑bit assembly like so:

```sh
vc --no-fold --no-dce --x86-64 -o simple_add.s tests/fixtures/simple_add.c
```

The generated file `simple_add.s` should match
`tests/fixtures/simple_add.s`:

```asm
main:
    pushl %ebp
    movl %esp, %ebp
    movl $7, %eax
    movl %eax, %eax
    ret
```

Any program compiled with the same options will produce assembly identical to
the corresponding `.s` file under `tests/fixtures`.
