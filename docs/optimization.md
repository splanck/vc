# Optimization Passes

See the [documentation index](index.md) for a list of all available pages.

The optimizer in **vc** operates on the intermediate representation (IR).
Six passes are currently available and are executed in order:
1. **Constant propagation** – replaces loads of variables whose values are
   known constants with immediate constants.
2. **Common subexpression elimination** – reuses results of identical
   computations.
3. **Inline expansion** – inlines functions containing up to four arithmetic
   instructions or just a `return` when they are marked `inline`.
4. **Constant folding** – evaluates arithmetic instructions whose operands are
   constants and replaces them with a single constant.
5. **Unreachable block elimination** – removes instructions that cannot be
   executed from the start of the function.
6. **Dead code elimination** – removes instructions that produce values which
   are never used and have no side effects.

Constant propagation tracks variables written with constants. When a later
load of such a variable is encountered, it becomes an immediate constant.
Long-double arithmetic results are propagated when both operands are known
constants, enabling subsequent folding.

Common subexpression elimination scans previously seen computations and
replaces duplicates with the existing value. This avoids emitting
identical arithmetic instructions multiple times.

Inline expansion now handles small inline functions containing up to
four arithmetic instructions or just a single `return`. The source
function must still be marked `inline`. When these criteria are met,
calls are replaced by the equivalent operations in the caller. This
reduces call overhead and allows the following passes to fold the
resulting expression.

If the optimizer cannot open the source file referenced by a candidate
function, a warning is printed and the call remains non-inline.

Constant folding evaluates arithmetic instructions whose operands are constant
values, replacing them with a single constant instruction.  Support now
includes the long double operations `IR_LFADD`, `IR_LFSUB`, `IR_LFMUL` and
`IR_LFDIV`, allowing their results to be simplified just like other
arithmetic.
For example, an expression such as `1.0L + 2.0L` is folded to a single
constant at compile time.
Intermediate results are checked for overflow; if a computation exceeds the
range of `long long` the compiler emits a `Constant overflow` diagnostic.

The unreachable block pass scans each function and removes any instructions
that cannot be reached from its `IR_FUNC_BEGIN`.  Blocks that follow an
unconditional branch or `IR_RETURN` are pruned even when they contain side
effects.

Dead code elimination scans the instruction stream and removes operations that
have no side effects and whose results are never referenced.

All optimizations are enabled by default. Constant folding and dead code
elimination may be toggled from the
command line:

```sh
vc --no-fold --no-dce -o out.s source.c
```
