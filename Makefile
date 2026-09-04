CC ?= gcc
CFLAGS = -std=gnu2x -Wall -Wextra -Wpedantic -Wno-unused-parameter -march=native -I./include
RELEASE_FLAGS = -O3 -funroll-loops
DEBUG_FLAGS = -g -Og -fsanitize=address -fsanitize=undefined

LDLIBS = -lncursesw -lm

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man/man1

SRCS = src/card.c src/player.c src/move.c src/game.c src/eval.c src/minmax.c src/front_cli.c src/front_tui.c src/engine.c src/backend_bitboard.c src/bitboard_rank_tables.c src/tournament.c src/profile.c
HDRS = include/card.h include/player.h include/move.h include/game.h include/eval.h include/minmax.h include/front_cli.h include/front_tui.h include/engine.h include/backend_bitboard.h include/bitboard_rank_tables.h include/tournament.h include/profile.h include/cte.h

.PHONY: all clean cte test run-test tests check test-bitboard run-test-bitboard bench run-bench install uninstall

all: cte test test-bitboard bench

build:
	@mkdir -p build

cte: build $(SRCS) $(HDRS) main.c
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o build/cte $(SRCS) main.c $(LDLIBS)

test: build $(SRCS) $(HDRS) test/test.c
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o build/test $(SRCS) test/test.c $(LDLIBS)

test-bitboard: build $(SRCS) $(HDRS) test/test_bitboard.c
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o build/test_bitboard $(SRCS) test/test_bitboard.c $(LDLIBS)

bench: build $(SRCS) $(HDRS) tools/bench_backends.c
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o build/bench_backends $(SRCS) tools/bench_backends.c $(LDLIBS)

run-test: test
	./build/test

run-test-bitboard: test-bitboard
	./build/test_bitboard

run-bench: bench
	./build/bench_backends

tests: run-test run-test-bitboard
check: tests

install: cte docs/cte.1
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 build/cte $(DESTDIR)$(BINDIR)/cte
	install -d $(DESTDIR)$(MANDIR)
	install -m 644 docs/cte.1 $(DESTDIR)$(MANDIR)/cte.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/cte
	rm -f $(DESTDIR)$(MANDIR)/cte.1

clean:
	rm -rf build
