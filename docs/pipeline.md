# vc Documentation

See the [documentation index](index.md) for a list of all available pages.

## Table of Contents

- [Pipeline Overview](#pipeline-overview)
- [Modules](#modules)
  - [preprocessor](#preprocessor)
  - [lexer](#lexer)
  - [parser](#parser)
  - [semantic](#semantic)
  - [ir](#ir)
  - [opt](#opt)
  - [regalloc](#regalloc)
  - [codegen](#codegen)
- [Optimization Passes](#optimization-passes)
  - [Constant folding](#constant-folding)
  - [Constant propagation](#constant-propagation)
  - [Dead code elimination](#dead-code-elimination)

This document describes the compilation pipeline, the role of each module
and the language features currently supported. Recent updates add basic
support for floating-point types and operations, along with preliminary
handling of inline function definitions.

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
Expands `#include` directives and simple object-like `#define` macros before
lexing.  Basic conditional directives (`#if`, `#ifdef`, `#ifndef`, `#elif`,
`#else` and `#endif`) are supported using simple expression evaluation with the
`defined` operator.  See [preprocessor workflow](preprocessor.md) for a more
detailed description.

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
using the `unsigned` keyword.  The IR now stores immediates in a
64-bit field so `long long` literals and arithmetic are handled without
truncation when generating x86-64 code.

Pointers work the same way and may be dereferenced using
`UNOP_DEREF`:

```c
int *p = &x;
return *p;
```

Variables may be declared with the `static` qualifier.  Static globals
behave like normal globals but are emitted as private symbols.  Static
locals are stored in the data section with unique names and require
constant initializers.

The `const` qualifier marks a variable as read-only after initialization.
Any attempt to assign to a `const` object results in a semantic error.
Declaring a `const` variable without an initializer is also a semantic error.

The `volatile` qualifier tells the compiler that a variable's value may change
unexpectedly.  Reads and writes to a `volatile` object are always emitted and
are not subject to certain optimizations.

The `restrict` qualifier may follow a `*` in pointer declarations.  It
promises that the pointer is the sole reference to its pointed-to object
for the lifetime of the pointer.  This lets the optimizer assume no aliasing
between `restrict` qualified pointers.

The `register` qualifier is accepted for compatibility.  The qualifier is
recorded in symbol tables but has no effect on code generation.  Objects
declared with `register` behave the same as ordinary locals or globals.

Structures and unions are declared with the `struct` or `union` keyword.
Members are accessed using `.` for objects or `->` when working through a pointer.
For structures each member has its own storage in the order declared.
Union members share the same storage with size determined by the largest field.

The compiler currently parses member access but does not verify which union member is active.

#### Typedef aliases

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

Function definitions may also be marked with the `inline` specifier. When
optimizations are enabled, very small inline functions may be expanded at
their call sites. The parser now records the `inline` keyword in symbol
tables so the semantic phase suppresses duplicate external definitions for
identical inline functions. When combined with `static`, the `static`
keyword must appear before `inline`.

### ir
Defines the IR structures used throughout the rest of the compiler.

#### Instruction set
The IR uses a straightforward three-address format. The operations defined in
[`include/ir_core.h`](../include/ir_core.h) include:

- `IR_CONST` for integer constants
- arithmetic ops `IR_ADD`, `IR_SUB`, `IR_MUL`, `IR_DIV`, `IR_MOD`
 - floating-point ops `IR_FADD`, `IR_FSUB`, `IR_FMUL`, `IR_FDIV`
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

Values spilled to stack slots are loaded back into a scratch register each time
they are referenced. Results whose assigned location is a stack slot are stored
after the defining instruction. One register is reserved for this purpose so
spills never overwrite active values.

```c
void regalloc_run(ir_builder_t *ir, regalloc_t *ra) {
    size_t max_id = ir->next_value_id;
    int *last = compute_last_use(ir, max_id);
    for each instruction {
        allocate_location(instr, free_regs, &free_count, ra);
        release_registers_at_last_use();
    }
}
```

### codegen
Emits assembly from the IR. Currently only x86 is supported. Each
function begins with a prolog that saves the caller frame pointer and
reserves stack space for spills. The corresponding epilog restores the
stack pointer, pops `%rbp`/`%ebp`, and emits `ret`. x86‑64 output keeps
the stack 16‑byte aligned.

## Optimization Passes

The `opt` module implements several transformations on the IR. These
passes run sequentially and may be disabled via command line options.
Constant propagation executes first, then constant folding and finally
dead code elimination.
See [optimization.md](optimization.md) for additional context.

### Constant folding
When both operands of an arithmetic instruction are known constants, the
compiler performs the calculation at compile time.  This folding now
also handles the floating-point operations `IR_FADD`, `IR_FSUB`,
`IR_FMUL` and `IR_FDIV` when their operands are constant values.

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

