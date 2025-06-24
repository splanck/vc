CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99
BIN = vc
SRC = src/main.c src/lexer.c src/ast.c src/parser.c src/semantic.c src/ir.c src/opt.c
HDR = include/token.h include/ast.h include/parser.h include/semantic.h include/ir.h include/opt.h
PREFIX ?= /usr/local
INCLUDEDIR ?= $(PREFIX)/include/vc

all: $(BIN)

$(BIN): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -Iinclude -o $@ $(SRC)

install: $(BIN)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -m 644 $(HDR) $(DESTDIR)$(INCLUDEDIR)
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(BIN) $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f $(BIN)

.PHONY: all clean install
