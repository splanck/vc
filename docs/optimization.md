# Optimization Passes

See the [documentation index](README.md) for a list of all available pages.

The optimizer in **vc** operates on the intermediate representation (IR).
Eight passes are currently available and are executed in order:
1. **Alias analysis** – assigns alias sets to memory operations.
2. **Constant propagation** – replaces loads of variables whose values are
   known constants with immediate constants.
3. **Common subexpression elimination** – reuses results of identical
   computations.
4. **Inline expansion** – inlines functions containing up to four arithmetic
   instructions or just a `return` when they are marked `inline`.
5. **Constant folding** – evaluates arithmetic instructions whose operands are
   constants and replaces them with a single constant.
6. **Loop-invariant code motion** – hoists computations whose operands do not
   change within a loop.
7. **Unreachable block elimination** – removes instructions that cannot be
   executed from the start of the function.
8. **Dead code elimination** – removes instructions that produce values which
   are never used and have no side effects.

Alias analysis assigns a unique identifier to every load and store. Named
variables share an alias set while operations through `restrict`-qualified
pointers receive their own. Later passes consult these identifiers so a store
only invalidates loads that may refer to the same set.

With distinct sets, constant propagation can remove more memory operations. As
an example,

```c
int test(int *restrict a, int *restrict b) {
    *a = 3;
    *b = 4;
    return *a;
}
```

reduces to `return 3;` because the final load is known not to alias the store
through `b`.

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
reduces call overhead and allows the following passes to fold the resulting expression.

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
Pointer arithmetic can also be simplified. When both operands of `IR_PTR_ADD`
or `IR_PTR_DIFF` are constants the result is computed during this pass,
eliminating the run-time address calculation.

The loop-invariant code motion pass looks for pure computations inside a loop
whose operands are defined outside the loop body. Such instructions are moved
before the loop header so they execute only once. This reduces the amount of
work performed during each iteration.

The unreachable block pass scans each function and removes any instructions
that cannot be reached from its `IR_FUNC_BEGIN`.  Blocks that follow an
unconditional branch or `IR_RETURN`/`IR_RETURN_AGG` are pruned even when they contain side
effects.

Dead code elimination scans the instruction stream and removes operations that
have no side effects and whose results are never referenced.

Pointers declared with the `restrict` qualifier participate in a simple alias
analysis.  Loads through restrict-qualified pointers are treated as pure
operations and may be merged by the common subexpression pass.  Stores through
such pointers no longer invalidate cached values of unrelated objects, allowing
more aggressive propagation.

All optimizations are enabled by default. Constant folding and dead code
elimination may be toggled from the
command line:

```sh
vc --no-fold --no-dce -o out.s source.c
```
