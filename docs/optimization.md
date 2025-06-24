# Optimization Passes

The optimizer in **vc** operates on the intermediate representation (IR).
Two passes are currently available:

- **Constant folding** – evaluates arithmetic instructions with constant
  operands and replaces them with a single constant.
- **Dead code elimination** – removes instructions that produce values
  which are never used and have no side effects.

Both optimizations are enabled by default. They may be toggled from the
command line:

```sh
vc --no-fold --no-dce -o out.s source.c
```
