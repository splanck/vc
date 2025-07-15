CC ?= gcc
PROJECT_ROOT := $(abspath ..)
CFLAGS ?= -Wall -Wextra -std=c99 -DPROJECT_ROOT=\"$(PROJECT_ROOT)\"
PREFIX ?= /usr/local
INCLUDEDIR ?= $(PREFIX)/include/vc

# determine if the host toolchain can build 32-bit objects
CAN_COMPILE_32 := $(shell $(CC) -m32 -xc /dev/null -o /dev/null \
    >/dev/null 2>&1 && echo yes || echo no)

SRC := src/stdio.c src/stdlib.c src/string.c src/syscalls.c src/file.c
HDR := include/stdarg.h include/stddef.h include/stdio.h \
       include/stdlib.h include/string.h
OBJ32 := $(SRC:src/%.c=src/%.32.o)
OBJ64 := $(SRC:src/%.c=src/%.64.o)

ifeq ($(CAN_COMPILE_32),yes)
all: libc32 libc64
else
all: libc64
	@echo "Notice: 32-bit compilation not available, skipping libc32 build"
endif

ifeq ($(CAN_COMPILE_32),yes)
libc32: libc32.a

libc32.a: $(OBJ32)
	ar rcs $@ $(OBJ32)

src/%.32.o: src/%.c
	$(CC) $(CFLAGS) -m32 -Iinclude -c $< -o $@
else
libc32 libc32.a:
	@echo "Notice: 32-bit compilation not available, skipping libc32 build"
endif

libc64: libc64.a

libc64.a: $(OBJ64)
	ar rcs $@ $(OBJ64)

src/%.64.o: src/%.c
	$(CC) $(CFLAGS) -m64 -Iinclude -c $< -o $@

install:
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -m 644 $(HDR) $(DESTDIR)$(INCLUDEDIR)

clean:
	rm -f src/*.32.o src/*.64.o libc32.a libc64.a

.PHONY: all libc32 libc64 clean install
