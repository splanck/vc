## Supported Language Features

See the [documentation index](README.md) for a list of all available pages.

This page lists the C constructs currently implemented in **vc**.  Each section
shows a short example and how to compile it.

## Table of Contents

- [Basic arithmetic](#basic-arithmetic)
- [Compound assignment](#compound-assignment)
- [Increment and decrement](#increment-and-decrement)
- [Floating-point arithmetic](#floating-point-arithmetic)
- [Complex numbers](#complex-numbers)
- [Bitwise operations](#bitwise-operations)
- [64-bit integers](#64-bit-integers)
- [Numeric constants](#numeric-constants)
- [Casts](#casts)
- [Function calls](#function-calls)
- [Returning structs](#returning-structs)
- [Variadic functions](#variadic-functions)
- [Loops](#loops)
- [Pointers](#pointers)
- [Arrays](#arrays)
- [Variable length arrays](#variable-length-arrays)
- [Compound literals](#compound-literals)
- [sizeof](#sizeof)
- [offsetof](#offsetof)
- [Global variables](#global-variables)
- [Break and continue](#break-and-continue)
- [Switch statements](#switch-statements)
- [Logical operators](#logical-operators)
- [Enum declarations](#enum-declarations)
- [Flexible array members](#flexible-array-members)
- [_Noreturn attribute](#_noreturn-attribute)

- Arrays with variable length support (block scope only), size inference from initializer lists and designated initializers using `[index]` or `.member` designators.
- Pointer arithmetic, including subtraction and increment operations.
- C-style casts using the `(type)expr` syntax for explicit conversions.
- Control flow with `for`, `while` and `do`/`while` loops.
- Global and external variable declarations.
- Floating-point types (including `long double`) and the boolean type via `_Bool`.
- Complex number types using the `_Complex` keyword with support for `+`, `-`, `*`, and `/` operators.
- 64-bit integer constants (hexadecimal and octal) and character or string literals with standard escape sequences. Octal escapes accept up to three digits and values beyond `\255` are clamped. Hexadecimal escapes consume at most two digits.
- Integer literals may use the suffixes `u`/`U` and `l`/`LL` in any order.
- Complete `struct` and `union` declarations with bit fields, enumeration types and typedefs.
- Union access tracking which reports an error if a different member is read after writing another.
- Compile-time assertions via `_Static_assert`.
- Functions marked with `_Noreturn` or the GNU `__attribute__((noreturn))` are treated as terminating calls.
- Wide character and string literals using `L'c'` and `L"text"`.
- Adjacent string literals are concatenated at compile time.
- Qualifiers such as `const` (requires an initializer), `volatile`, `restrict` and `register`.
- Statements like `break`, `continue`, `switch` with `case`/`default` labels, variadic functions using `...` (floating-point arguments are handled correctly), labels and `goto`.
- Inline function definitions.

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

### Complex numbers
The `_Complex` keyword introduces complex floating-point types. Complex
literals use `a+bi` syntax with `i` denoting the imaginary part.
Addition, subtraction, multiplication and division are supported.

```c
/* complex_add.c */
double _Complex main() {
    double _Complex a = 1.0 + 2.0i;
    double _Complex b = 3.0 - 1.0i;
    return a + b;
}
```
Compile with:
```sh
vc -o complex_add.s complex_add.c
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

Integer literals may optionally end with `u`/`U` to make the
constant unsigned and `l`/`L` or `ll`/`LL` to select `long` or
`long long`.  These suffixes can be mixed in any order such as
`1ul` or `3llu`.

### Casts
```c
/* cast.c */
int main() {
    double d = 3.7;
    return (int)d;
}
```
Compile with:
```sh
vc -o cast.s cast.c
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

### Returning structs
```c
/* struct_ret.c */
struct pair { int a; int b; };

struct pair make_pair(int x, int y) {
    struct pair p = { x, y };
    return p;
}

int main() {
    struct pair p = make_pair(1, 2);
    return p.a + p.b;
}
```
Compile with:
```sh
vc -o struct_ret.s struct_ret.c
```

### Variadic functions
Functions may accept a variable number of arguments by placing `...` at the end
of the parameter list.  The extra parameters are pushed on the stack and can be
accessed with the standard `<stdarg.h>` macros.  Both integer and
floating‑point arguments are supported.
```c
/* var_args.c */
#include <stdarg.h>
double sumd(int n, ...) {
    va_list ap;
    va_start(ap, n);
    double t = 0;
    for (int i = 0; i < n; i++)
        t += va_arg(ap, double);
    va_end(ap);
    return t;
}
int main() { return (int)sumd(2, 1.0, 2.0); }
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
char *concat = "foo" "bar"; /* becomes "foobar" */
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
than the machine word size. If the pointed-to type has size zero, the
difference is defined to be zero.

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

Pointer subtraction using a variable offset works the same way:

```c
/* ptr_var_sub.c */
int main() {
    int nums[3] = {1, 2, 3};
    int i = 2;
    int *p = nums + i;
    p = p - i;
    return *p;
}
```
Compile with:
```sh
vc -o ptr_var_sub.s ptr_var_sub.c
```

Pointers can also be compared when they refer to elements of the same array:

```c
/* ptr_compare.c */
int main() {
    int arr[2];
    int *p1 = arr;
    int *p2 = arr + 1;
    return p1 < p2;
}
```
Compile with:
```sh
vc -o ptr_compare.s ptr_compare.c
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

#### Variable length arrays
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

### offsetof
`offsetof` returns the byte offset of a struct or union member.

```c
/* off.c */
struct S { int a; char b; };
int main() {
    return offsetof(struct S, b);
}
```
Compile with:
```sh
vc -o off.s off.c
```

### _Alignof and alignas
`_Alignof` yields the required alignment of a type. The `alignas` specifier
sets a custom alignment for objects.

```c
/* align.c */
alignas(16) int g;
int main() {
    return _Alignof(int);
}
```
Compile with:
```sh
vc -o align.s align.c
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

Pointers may also be initialized with the address of another symbol:

```c
/* global_ptr.c */
int x;
int *p = &x;
int main() {
    return *p;
}
```
Compile with:
```sh
vc -o global_ptr.s global_ptr.c
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

### Flexible array members
Structures may end with a member declared using empty brackets. The flexible
member occupies no space in `sizeof` calculations and allows extra storage to be
appended at runtime. Such members must appear last in the struct and do not
contribute to the computed type size.

```c
/* flex_size.c */
struct Flex { int len; int data[]; };
int main() {
    return sizeof(struct Flex);
}
```
Compile with:
```sh
vc -o flex_size.s flex_size.c
```

### Typedef declarations
```c
/* typedef_example.c */
typedef struct Point { int x; int y; } Point;
int main() {
    Point p = {1, 2};
    return p.x + p.y;
}
```
Compile with:
```sh
vc -o typedef_example.s typedef_example.c
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

### _Static_assert
Compile-time assertions may be written using the `_Static_assert(expr, "msg")`
syntax. The expression must be a constant expression and triggers an error with
the provided message when it evaluates to zero.

```c
/* assert_example.c */
_Static_assert(1 < 2, "math works");
int main() { return 0; }
```
Compile with:
```sh
vc -o assert_example.s assert_example.c
```

### _Noreturn attribute
A function may be marked as not returning using the `_Noreturn` keyword or the
GNU `__attribute__((noreturn))` syntax. Calls to such functions are treated as
terminating the current control path.

```c
/* noreturn_example.c */
_Noreturn void die(void);

void foo(int n) {
    if (n < 0)
        die();
    return;
}
```
Compile with:
```sh
vc -o noreturn_example.s noreturn_example.c
```

### Variable qualifiers
Several qualifiers modify how variables are accessed or stored.

- `const` objects cannot be written after initialization. File scope `const`
  variables must have an initializer.
- `volatile` tells the compiler that a variable may change outside the program's
  control and prevents certain optimizations.
- `restrict` promises that, for the lifetime of the pointer, only it will be
  used to access the referenced object.
- `register` suggests keeping the variable in a CPU register when possible.

```c
/* qualifiers.c */
const int c = 5;
volatile int v;
int *restrict p;

int main(void) {
    register int r = c;
    v = r;
    p = &v;
    return *p;
}
```
Compile with:
```sh
vc -o qualifiers.s qualifiers.c
```

Pointers qualified with `restrict` can be passed to functions to promise
that they do not alias each other. This enables additional optimizations.

```c
/* restrict_ptr.c */
int sum(int *restrict a, int *restrict b) {
    return *a + *b;
}
```
Compile with:
```sh
vc -o restrict_ptr.s restrict_ptr.c
```

