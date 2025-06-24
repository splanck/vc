# Optimization Passes

The optimizer in **vc** operates on the intermediate representation (IR).
Three passes are currently available:

- **Constant folding** – evaluates arithmetic instructions with constant
  operands and replaces them with a single constant.
- **Constant propagation** – replaces loads of variables whose values are
  known constants with immediate constants.
- **Dead code elimination** – removes instructions that produce values
  which are never used and have no side effects.

All optimizations are enabled by default. Constant folding and dead code
elimination may be toggled from the
command line:

```sh
vc --no-fold --no-dce -o out.s source.c
```
