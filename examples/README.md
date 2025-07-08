# Example Programs

This directory contains small C programs demonstrating the **vc** compiler.

## Building the compiler

Run `make` from the repository root to build the `vc` binary. After that,
change into this directory to compile the examples.

## Compiling the examples

Each program can be compiled individually using `../vc` with the `--link`
option:

```sh
../vc --link -o hello hello.c
```

Replace `hello` and `hello.c` with the desired output name and source file.

To compile every example at once run the helper script:

```sh
./build_examples.sh
```

## Programs

### hello.c
Prints "Hello, world!". After building run:

```sh
./hello
```

### calc.c
Shows basic arithmetic and a loop that sums numbers 1..10. Run the program
with:

```sh
./calc
```

### file_io.c
Writes a string to `example.txt` and then reads it back. Run with:

```sh
./file_io
```
