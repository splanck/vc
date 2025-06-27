# Optimization Passes

See the [documentation index](index.md) for a list of all available pages.

The optimizer in **vc** operates on the intermediate representation (IR).
Three passes are currently available and are executed in order:
1. **Constant propagation** – replaces loads of variables whose values are
   known constants with immediate constants.
2. **Constant folding** – evaluates arithmetic instructions whose operands are
   constants and replaces them with a single constant.
3. **Dead code elimination** – removes instructions that produce values which
   are never used and have no side effects.

Constant propagation tracks variables written with constants. When a later
load of such a variable is encountered, it becomes an immediate constant.

Constant folding evaluates arithmetic instructions whose operands are constant
values, replacing them with a single constant instruction.

Dead code elimination scans the instruction stream and removes operations that
have no side effects and whose results are never referenced.

All optimizations are enabled by default. Constant folding and dead code
elimination may be toggled from the
command line:

```sh
vc --no-fold --no-dce -o out.s source.c
```
