# vc Documentation

This document describes the compilation pipeline, the role of each module
and the language features currently supported. Recent updates add basic
support for floating-point types and operations.

## Pipeline Overview

`vc` processes source code through several stages:

1. **Preprocessor** – handles `#include` directives and expands simple `#define` macros.
2. **Lexer** – converts the input into a stream of tokens.
3. **Parser** – builds an abstract syntax tree from the tokens.
4. **Semantic analyzer** – checks the AST and produces an intermediate representation (IR).
5. **Optimizer** – performs optional transformations on the IR.
6. **Register allocator** – assigns machine registers.
7. **Code generator** – emits target assembly.

The modules described below implement these steps.

## Modules

### preprocessor
Expands `#include` directives and simple object-like `#define` macros before lexing.
Only textual substitution is performed; conditional directives are not supported.

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
    int is_typedef;  /* alias flag */
    type_kind_t alias_type;
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
char c = 'a';   /* adds 'c' with TYPE_CHAR */
int y = 5;      /* evaluates 5 and stores to 'y' */
```

Additional built-in types such as `short`, `long`, `long long` and
`bool` are recognized.  Each integer type also has an unsigned variant
using the `unsigned` keyword.

Pointers work the same way and may be dereferenced using
`UNOP_DEREF`:

```c
int *p = &x;
return *p;
```
Function pointers can be declared with a parenthesized `*` declarator:

```c
int (*fp)(int, int);
fp = &some_func;
```

Variables may be declared with the `static` qualifier.  Static globals
behave like normal globals but are emitted as private symbols.  Static
locals are stored in the data section with unique names and require
constant initializers.

Struct and union objects are declared similarly using the new
`struct` and `union` keywords. Member access is parsed with `.` or
`->`:

```c
struct Point { int x; int y; };
struct Point p;
p.x = 3;
return p.x;
```

#### Typedef aliases

Type aliases can be introduced using the `typedef` keyword.  Only
aliases of built-in types are supported currently:

```c
typedef int myint;
myint val;
```

Typedef declarations are stored in the symbol table and may appear at
global scope.

#### Type checking and functions

`check_expr` verifies binary operations and resolves function calls by
consulting the symbol table.  A call returns the function's declared
type:

```c
int foo() { return 3; }
int main() { return foo(); }
```

Functions may be declared before they are defined using prototypes:

```c
int bar(int);
int main() { return bar(1); }
int bar(int x) { return x + 1; }
```

Prototypes are stored in the function symbol table. Later definitions
must match the previously declared signature.

Any mismatch results in `error_print` reporting the source
location of the failure.

### ir
Defines the IR structures used throughout the rest of the compiler.

#### Instruction set
The IR uses a straightforward three-address format. The operations defined in
[`include/ir.h`](../include/ir.h) include:

- `IR_CONST` for integer constants
 - arithmetic ops `IR_ADD`, `IR_SUB`, `IR_MUL`, `IR_DIV`, `IR_MOD`
- comparison ops `IR_CMPEQ`, `IR_CMPNE`, `IR_CMPLT`, `IR_CMPGT`, `IR_CMPLE`, `IR_CMPGE`
- global data directives `IR_GLOB_STRING`, `IR_GLOB_VAR`
- variable access `IR_LOAD`, `IR_STORE`, `IR_LOAD_PARAM`, `IR_STORE_PARAM`
- pointer ops `IR_ADDR`, `IR_LOAD_PTR`, `IR_STORE_PTR`, `IR_PTR_ADD`,
  `IR_PTR_DIFF`
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

`regalloc_run` walks the instruction stream, assigning either a register or a
new stack slot to each produced value. A small pool of registers is kept in a
stack (`free_regs`). When no registers remain the next slot is used. The
allocator frees registers by consulting the table of last uses generated by
`compute_last_use`.

`regalloc_free` releases the mapping table created by `regalloc_run`. The
helper `regalloc_reg_name` converts register indices to the correct physical
name for 32‑ or 64‑bit mode, which is toggled via `regalloc_set_x86_64`.

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
- `for`, `while` and `do`/`while` loops
- Loop initialization may declare a variable
- Pointers
- Arrays
- Compound assignment operators (`+=`, `-=`, `*=`, `/=`, `%=`)
- Increment and decrement operators (`++`, `--`)
- Logical operators `&&`, `||` and `!`
- Conditional operator (`?:`)
- Bitwise operators (`&`, `|`, `^`, `<<`, `>>` and compound forms)
- Floating-point types (`float`, `double`)
- `sizeof` operator
- Global variables
- `break` and `continue` statements
- Labels and `goto`

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

### Compound assignment
```c
/* compound.c */
int main() {
    int x = 1;
    x += 2;
    return x;
}
```
Compile with:
```sh
vc -o compound.s compound.c
```

### Increment and decrement
```c
/* incdec.c */
int main() {
    int i = 0;
    ++i;
    i--;
    return i;
}
```
Compile with:
```sh
vc -o incdec.s incdec.c
```

### Floating-point arithmetic
```c
/* float_add.c */
float main() {
    float a = 1.0f;
    float b = 2.0f;
    return a + b;
}
```
Compile with:
```sh
vc -o float_add.s float_add.c
```

### Bitwise operations
```c
/* bitwise.c */
int main() {
    int x = 1;
    x <<= 2;
    x |= 3;
    return x & 1;
}
```
Compile with:
```sh
vc -o bitwise.s bitwise.c
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

Function pointers are also supported:

```c
int add(int a, int b) { return a + b; }
int (*fp)(int, int);
int main() {
    fp = add;
    return fp(2, 3);
}
```
Compile with:
```sh
vc -o func_ptr.s func_ptr.c
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

`do`/`while` loops are also supported:
```c
/* do_loop.c */
int main() {
    int i = 0;
    do
        i = i + 1;
    while (i < 3);
    return i;
}
```
Compile with:
```sh
vc -o do_loop.s do_loop.c
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

#### Pointer arithmetic
```c
/* ptr_arith.c */
int main() {
    int a[3] = {1, 2, 3};
    int *p = a;
int second = *(p + 1);
p = p + 2;
return *(p - 1);
}
```
Compile with:
```sh
vc -o ptr_arith.s ptr_arith.c
```
Pointer subtraction of two pointers is also supported and returns the
element distance between them.

### Arrays
```c
/* array.c */
int main() {
    int a[2];
    a[0] = 1;
    a[1] = 2;
    return a[0] + a[1];
}
```
Compile with:
```sh
vc -o array.s array.c
```

Arrays can also be initialized using an initializer list when declared:

```c
/* array_init.c */
int main() {
    int nums[3] = {1, 2, 3};
    return nums[0];
}
```
Compile with:
```sh
vc -o array_init.s array_init.c
```

### sizeof
`sizeof` returns the number of bytes for a type or expression without
evaluating the expression.

```c
/* sz.c */
int main() {
    int x;
    return sizeof(int) + sizeof(x);
}
```
Compile with:
```sh
vc -o sz.s sz.c
```

### Global variables
```c
/* global.c */
int x = 5;
int main() {
    return x;
}
```
Compile with:
```sh
vc -o global.s global.c
```

### Break and continue
```c
/* loop_control.c */
int main() {
    int i;
    for (i = 0; i < 3; i = i + 1) {
        if (i == 1)
            continue;
        if (i == 2)
            break;
    }
    return i;
}
```
Compile with:
```sh
vc -o loop_control.s loop_control.c
```

### For loop variable declarations
```c
/* for_decl.c */
int main() {
    int sum = 0;
    for (int i = 0; i < 3; i = i + 1)
        sum = sum + i;
    return sum;
}
```
Compile with:
```sh
vc -o for_decl.s for_decl.c
```

### Logical operators
```c
/* logical.c */
int main() {
    int x = 1;
    int y = 0;
    if (x && !y)
        return 1;
    else
        return 0;
}
```
Compile with:
```sh
vc -o logical.s logical.c
```

### Switch statements
```c
/* switch_example.c */
int main() {
    int x = 2;
    switch (x) {
    case 1:
        return 10;
    case 2:
        return 20;
    default:
        return 0;
    }
}
```
Compile with:
```sh
vc -o switch.s switch_example.c
```

### Enum declarations
```c
/* enum_example.c */
enum Colors {
    RED,
    GREEN = 3,
    BLUE
};
int main() {
    return GREEN;
}
```
Compile with:
```sh
vc -o enum_example.s enum_example.c
```

### Labels and goto
```c
/* goto_example.c */
int main() {
    int i = 0;
start:
    if (i == 3)
        goto end;
    i = i + 1;
    goto start;
end:
    return i;
}
```
Compile with:
```sh
vc -o goto.s goto_example.c
```

## Command-line Options

The compiler supports the following options:

- `-o`, `--output <file>` – path for the generated assembly.
- `-h`, `--help` – display help text and exit.
- `-v`, `--version` – print version information and exit.
- `--no-fold` – disable constant folding.
- `--no-dce` – disable dead code elimination.
- `--no-cprop` – disable constant propagation.
- `--x86-64` – generate 64‑bit x86 assembly.
- `--dump-asm` – print the generated assembly to stdout instead of creating a file.
- `--dump-ir` – print the IR to stdout before code generation.
- `-O<N>` – set optimization level (0 disables all passes).

Use `vc -o out.s source.c` to compile a file, or `vc --dump-asm source.c` to
print the assembly to the terminal.

## Preprocessor Usage

The preprocessor runs automatically before the lexer. It supports `#include "file"`
to insert the contents of another file and simple object-like `#define` macros:

```c
#define VAL 3
#include "header.h"
int main() { return VAL; }
```

Macro expansion is purely textual; macros inside strings or comments are not
recognized and conditional directives are ignored.

## Compiling a Simple Program

Example source files can be found under `tests/fixtures`. The simplest is
`simple_add.c`, which returns the result of `1 + 2 * 3`. Compile it with
optimizations disabled and request 64‑bit assembly like so:

```sh
vc -O0 --x86-64 -o simple_add.s tests/fixtures/simple_add.c
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
