## Supported Language Features

See the [documentation index](index.md) for a list of all available pages.

## Table of Contents

- [Basic arithmetic](#basic-arithmetic)
- [Compound assignment](#compound-assignment)
- [Increment and decrement](#increment-and-decrement)
- [Floating-point arithmetic](#floating-point-arithmetic)
- [Bitwise operations](#bitwise-operations)
- [64-bit integers](#64-bit-integers)
- [Numeric constants](#numeric-constants)
- [Function calls](#function-calls)
- [Variadic functions](#variadic-functions)
- [Loops](#loops)
- [Pointers](#pointers)
- [Arrays](#arrays)
- [Compound literals](#compound-literals)
- [sizeof](#sizeof)
- [Global variables](#global-variables)
- [Break and continue](#break-and-continue)
- [Switch statements](#switch-statements)
- [Logical operators](#logical-operators)
- [Enum declarations](#enum-declarations)

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
- `switch` statements with `case` and `default`
- Bitwise operators (`&`, `|`, `^`, `<<`, `>>` and compound forms)
- Floating-point types (`float`, `double`, `long double`)
- `sizeof` operator
- Global variables
- Variadic functions using `...`
- `extern` declarations for globals and function prototypes
- `break` and `continue` statements
- Labels and `goto`
- `struct` and `union` objects with member assignments
- Bit-field members using `type name : width`
- Object-like and multi-parameter `#define` macros with recursive expansion
- `#undef` to remove a previously defined macro
- Conditional preprocessing directives (`#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`)
- 64-bit integer literals and arithmetic when using `long long`
- Hexadecimal (`0x`) and octal (leading `0`) integer literals
- String literals which evaluate to a `char *`
- Character and string literal escapes such as `\n`, `\t`, `\r`, `\b`, `\f`,
  `\v` along with octal (`\123`) and hexadecimal (`\x7F`) forms

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
    float a = 1;
    float b = 2;
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

### 64-bit integers
```c
/* ll_const.c */
int main() {
    long long a = 5000000000;
    return a + 5;
}
```
Compile with:
```sh
vc -o ll_const.s ll_const.c
vc --x86-64 -o ll_const_x86-64.s ll_const.c
```

### Numeric constants

Integer literals may be written in decimal, hexadecimal or octal form.
Numbers beginning with `0x` or `0X` are parsed as hexadecimal. Numbers
starting with `0` but not `0x`/`0X` are treated as octal. All other
numbers are interpreted as decimal.

```c
int a = 0x2a;  /* 42 in hex */
int b = 0755;  /* octal */
int c = 10;    /* decimal */
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

### Variadic functions
Functions may accept a variable number of arguments by placing `...` at the end
of the parameter list.  The extra parameters are pushed on the stack and can be
accessed with the standard `<stdarg.h>` macros.  At present only integer and
pointer values are handled reliably; passing floating‑point arguments is not
yet supported.
```c
/* var_args.c */
#include <stdarg.h>
int sum(int n, ...) {
    va_list ap;
    va_start(ap, n);
    int t = 0;
    for (int i = 0; i < n; i++)
        t += va_arg(ap, int);
    va_end(ap);
    return t;
}
int main() { return sum(3, 1, 2, 3); }
```
Compile with:
```sh
vc -o var_args.s var_args.c
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

String literals evaluate to the address of their storage and may be
assigned to `char *` variables:

```c
char *msg = "hi";
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
Pointer offsets are scaled by the size of the pointed-to type rather
than the machine word size.

Pointer variables may also be incremented or decremented with `++` and
`--`.  These operations are equivalent to adding or subtracting one
element and use the same scaling rules.

```c
/* ptr_inc.c */
int main() {
    int nums[2] = {1, 2};
    int *p = nums;
    ++p;
    return *p;
}
```
Compile with:
```sh
vc -o ptr_inc.s ptr_inc.c
```

#### Function pointers
Pointers may reference functions and use the standard `(*name)(...)` notation.
They can be called through just like normal identifiers.

```c
/* func_ptr.c */
int add(int a, int b) { return a + b; }

int main() {
    int (*op)(int, int) = add;
    return op(1, 2);
}
```
Compile with:
```sh
vc -o func_ptr.s func_ptr.c
```

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
If the size between the brackets is omitted, it is inferred from the number of
initializer elements:

```c
/* array_infer_size.c */
int main() {
    int nums[] = {1, 2, 3};
    return nums[1];
}
```
Compile with:
```sh
vc -o array_init.s array_init.c
```

Variable length arrays may use any runtime expression between the brackets:

```c
/* vla.c */
int main(int n) {
    int buf[n + 2];
    buf[0] = 1;
    return buf[n];
}
```
Compile with:
```sh
vc -o vla.s vla.c
```

Designated initializers can specify the index for each element:

```c
/* array_designate.c */
int main() {
    int nums[5] = { [2] = 4, [4] = 9 };
    return nums[2] + nums[4];
}
```
Compile with:
```sh
vc -o array_designate.s array_designate.c
```

### Compound literals
Compound literals create a temporary object using `(type){...}` syntax.

```c
/* compound_literal.c */
int main() {
    return (int){5};
}
```
Compile with:
```sh
vc -o compound_literal.s compound_literal.c
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

### Switch statements
```c
/* switch.c */
int main() {
    int x = 1;
    switch (x) {
    case 0:
        return 5;
    case 1:
        return 10;
    default:
        return -1;
    }
}
```
Compile with:
```sh
vc -o switch.s switch.c
```

Only one `default` label may appear within the switch block. If provided,
its statement executes when none of the `case` values match.

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

### Enum variables
```c
/* enum_var.c */
enum Colors { RED, GREEN, BLUE };
enum Colors c;
int main() {
    c = GREEN;
    return c;
}
```
Compile with:
```sh
vc -o enum_var.s enum_var.c
```

### Struct declarations
```c
/* struct_example.c */
struct Point { int x; int y; };
int main() {
    struct Point p;
    p.x = 1;
    p.y = 2;
    return p.x + p.y;
}
```
Compile with:
```sh
vc -o struct_example.s struct_example.c
```

### Struct pointers
```c
/* struct_ptr.c */
struct Point { int x; int y; };
int main() {
    struct Point p = {1, 2};
    struct Point *pp = &p;
    return pp->y;
}
```
Compile with:
```sh
vc -o struct_ptr.s struct_ptr.c
```

### Struct designators
```c
/* struct_designate.c */
struct Point { int x; int y; };
int main() {
    struct Point p = { .y = 5, .x = 1 };
    return p.y - p.x;
}
```
Compile with:
```sh
vc -o struct_designate.s struct_designate.c
```
### Union declarations
```c
/* union_example.c */
union { int i; char c; } u;
int main() {
    u.i = 65;
    return u.i;
}
```
Compile with:
```sh
vc -o union_example.s union_example.c
```

Another example assigns a character and accesses the same storage via a
different member:

```c
/* union_char.c */
union { int i; char c; } u;
int main() {
    u.c = 'A';
    return u.c;
}
```
Compile with:
```sh
vc -o union_char.s union_char.c
```

### Bit-fields

Bit-field members have the syntax `type name : width;`. Consecutive fields of
the same type share storage and are packed starting from the least significant
bits of the underlying type. When the combined width exceeds that type's size,
a new storage unit is allocated. A field declared with width `0` forces the next
bit-field to begin in a new unit.

```c
/* bitfield_example.c */
struct Flags {
    unsigned a : 1;
    unsigned b : 2;
    unsigned pad : 5;
};
int main() {
    struct Flags f = {1, 3, 0};
    return f.a + f.b;
}
```
Compile with:
```sh
vc -o bitfield_example.s bitfield_example.c
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

