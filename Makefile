CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99
BIN = vc
SRC = src/main.c

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	rm -f $(BIN)

.PHONY: all clean
