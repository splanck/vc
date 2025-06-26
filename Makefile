CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99
OPTFLAGS ?=
BIN = vc
# The resulting binary accepts -c/--compile to assemble objects using cc
# Core compiler sources

CORE_SRC = src/main.c src/cli.c src/lexer.c src/ast.c src/parser.c src/symtable.c src/parser_expr.c \
           src/parser_stmt.c src/parser_types.c src/semantic_expr.c src/semantic_stmt.c src/semantic_global.c src/error.c src/ir.c src/codegen.c src/regalloc.c src/regalloc_x86.c src/strbuf.c src/util.c \
           src/vector.c src/ir_dump.c src/label.c src/preproc.c

# Optional optimization sources
OPT_SRC = src/opt.c
# Additional sources can be specified by the user
EXTRA_SRC ?=
# Final source list
SRC = $(CORE_SRC) $(OPT_SRC) $(EXTRA_SRC)
HDR = include/token.h include/ast.h include/parser.h include/symtable.h include/semantic.h     include/semantic_expr.h include/semantic_stmt.h include/semantic_global.h \
    include/ir.h include/ir_dump.h include/opt.h include/codegen.h include/strbuf.h \
    include/util.h include/cli.h include/vector.h include/regalloc_x86.h include/label.h include/error.h include/preproc.h include/parser_types.h
PREFIX ?= /usr/local
INCLUDEDIR ?= $(PREFIX)/include/vc
MANDIR ?= $(PREFIX)/share/man

all: $(BIN)

test: $(BIN)
	./tests/run_tests.sh

$(BIN): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -o $@ $(SRC)

install: $(BIN)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -m 644 $(HDR) $(DESTDIR)$(INCLUDEDIR)
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(BIN) $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m 644 man/vc.1 $(DESTDIR)$(MANDIR)/man1/

clean:
	rm -f $(BIN)

.PHONY: all clean install test
