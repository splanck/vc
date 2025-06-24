CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99
OPTFLAGS ?=
BIN = vc
# Core compiler sources
CORE_SRC = src/main.c src/lexer.c src/ast.c src/parser.c src/semantic.c src/ir.c src/codegen.c
# Optional optimization sources
OPT_SRC = src/opt.c
# Additional sources can be specified by the user
EXTRA_SRC ?=
# Final source list
SRC = $(CORE_SRC) $(OPT_SRC) $(EXTRA_SRC)
HDR = include/token.h include/ast.h include/parser.h include/semantic.h include/ir.h include/opt.h include/codegen.h
PREFIX ?= /usr/local
INCLUDEDIR ?= $(PREFIX)/include/vc

all: $(BIN)

$(BIN): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(OPTFLAGS) -Iinclude -o $@ $(SRC)

install: $(BIN)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -m 644 $(HDR) $(DESTDIR)$(INCLUDEDIR)
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(BIN) $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f $(BIN)

.PHONY: all clean install
