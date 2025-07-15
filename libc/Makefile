CC ?= gcc
PROJECT_ROOT := $(abspath ..)
CFLAGS ?= -Wall -Wextra -std=c99 -DPROJECT_ROOT=\"$(PROJECT_ROOT)\"

SRC := src/stdio.c src/stdlib.c src/string.c src/syscalls.c
OBJ32 := $(SRC:src/%.c=src/%.32.o)
OBJ64 := $(SRC:src/%.c=src/%.64.o)

all: libc32 libc64

libc32: libc32.a

libc64: libc64.a

libc32.a: $(OBJ32)
	ar rcs $@ $(OBJ32)

libc64.a: $(OBJ64)
	ar rcs $@ $(OBJ64)

src/%.32.o: src/%.c
	$(CC) $(CFLAGS) -m32 -Iinclude -c $< -o $@

src/%.64.o: src/%.c
	$(CC) $(CFLAGS) -m64 -Iinclude -c $< -o $@

clean:
	rm -f src/*.32.o src/*.64.o libc32.a libc64.a

.PHONY: all libc32 libc64 clean
