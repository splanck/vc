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
Performs type checking and converts the AST into IR.  The
implementation in [`src/semantic.c`](../src/semantic.c) relies on a
simple symbol table defined in [`include/semantic.h`](../include/semantic.h).

The table tracks local variables, function parameters and global
symbols.  Each `symbol_t` entry stores the name, its `type_kind_t` and
an optional parameter index:

```c
typedef struct symbol {
    char *name;
    type_kind_t type;
    int param_index; /* -1 for locals */
    struct symbol *next;
} symbol_t;
```

Symbols are inserted with `symtable_add` or
`symtable_add_global`, while `symtable_lookup` retrieves an entry.  The
semantic checker uses these helpers when processing function bodies and
global declarations.

#### Variable declarations

`check_stmt` handles `STMT_VAR_DECL` nodes.  The variable is added to
the current table and any initializer is evaluated and stored:

```c
int x;          /* adds 'x' with TYPE_INT */
int y = 5;      /* evaluates 5 and stores to 'y' */
```

Pointers work the same way and may be dereferenced using
`UNOP_DEREF`:

```c
int *p = &x;
return *p;
```

#### Type checking and functions

`check_expr` verifies binary operations and resolves function calls by
consulting the symbol table.  A call returns the function's declared
type:

```c
int foo() { return 3; }
int main() { return foo(); }
```

Any mismatch results in `semantic_print_error` reporting the source
location of the failure.

### ir
Defines the IR structures used throughout the rest of the compiler.

#### Instruction set
The IR uses a straightforward three-address format. The operations defined in
[`include/ir.h`](../include/ir.h) include:

- `IR_CONST` for integer constants
- arithmetic ops `IR_ADD`, `IR_SUB`, `IR_MUL`, `IR_DIV`
- comparison ops `IR_CMPEQ`, `IR_CMPNE`, `IR_CMPLT`, `IR_CMPGT`, `IR_CMPLE`, `IR_CMPGE`
- global data directives `IR_GLOB_STRING`, `IR_GLOB_VAR`
- variable access `IR_LOAD`, `IR_STORE`, `IR_LOAD_PARAM`, `IR_STORE_PARAM`
- pointer ops `IR_ADDR`, `IR_LOAD_PTR`, `IR_STORE_PTR`
- function and call ops `IR_RETURN`, `IR_CALL`, `IR_FUNC_BEGIN`, `IR_FUNC_END`
- control flow `IR_BR`, `IR_BCOND`, `IR_LABEL`

IR instructions are appended sequentially using the builder API. A tiny
function returning `2 * 3` would be built as:

```c
ir_builder_t b;
ir_builder_init(&b);
ir_value_t a = ir_build_const(&b, 2);
ir_value_t bval = ir_build_const(&b, 3);
ir_value_t res = ir_build_binop(&b, IR_MUL, a, bval);
ir_build_return(&b, res);
```

### opt
Holds optimization passes such as constant folding and dead code elimination.

### regalloc
Handles register allocation for the backend.

#### Linear-scan allocator
The allocator in [`src/regalloc.c`](../src/regalloc.c) performs a single pass
over the instruction list. It first records the final index at which each value
is used. During the scan each destination value gets a register if one is
available; otherwise a new stack slot is assigned. Registers are released once
their value's last use is reached.

```c
void regalloc_run(ir_builder_t *ir, regalloc_t *ra) {
    int *last = compute_last_use(ir, ir->next_value_id);
    for each instruction {
        if (needs_location(dest))
            allocate_register_or_spill();
        release_registers_at_last_use();
    }
}
```
`regalloc_reg_name` converts register indices to the appropriate physical name
for 32‑ or 64‑bit mode.

### codegen
Emits assembly from the IR. Currently only x86 is supported.

## Optimization Passes

The `opt` module implements several transformations on the IR. These
passes run sequentially and may be disabled via command line options.
See [optimization.md](optimization.md) for additional context.

### Constant folding
When both operands of an arithmetic instruction are known constants, the
compiler performs the calculation at compile time.

Before:
```text
v1 = CONST 2
v2 = CONST 3
v3 = MUL v1, v2
```
After:
```text
v3 = CONST 6
```

### Constant propagation
Loads of variables whose values are known constants are replaced with
those constants.

Before:
```text
v1 = CONST 5
STORE x, v1
v2 = LOAD x
```
After:
```text
v1 = CONST 5
STORE x, v1
v2 = CONST 5
```

### Dead code elimination
Instructions that produce values never used and have no side effects are
removed.

Before:
```text
v1 = MUL 2, 3
v2 = ADD 1, 4
RETURN v2
```
After:
```text
v2 = ADD 1, 4
RETURN v2
```

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
