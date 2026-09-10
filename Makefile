CC ?= gcc
CFLAGS = -std=gnu2x -Wall -Wextra -Wpedantic -Wno-unused-parameter -march=native -I./include
RELEASE_FLAGS = -O3 -funroll-loops
DEBUG_FLAGS = -g -Og -fsanitize=address -fsanitize=undefined

LDLIBS = -lncursesw -lm

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man/man1

# Core engine sources (zero UI, zero ncurses)
ENGINE_SRCS = src/card.c src/player.c src/move.c src/game.c src/eval.c src/minmax.c src/engine.c src/backend_bitboard.c src/bitboard_rank_tables.c
# Tournament & profile extension
TOURN_SRCS  = $(ENGINE_SRCS) src/tournament.c src/profile.c
# Full application sources
SRCS        = $(TOURN_SRCS) src/front_cli.c src/front_tui.c
HDRS        = include/card.h include/player.h include/move.h include/game.h include/eval.h include/minmax.h include/front_cli.h include/front_tui.h include/engine.h include/backend_bitboard.h include/bitboard_rank_tables.h include/tournament.h include/profile.h include/cte.h

# Object directories
OBJDIR_REL = build/obj_rel
OBJDIR_DBG = build/obj_dbg

# Release objects for cte binary
OBJS_REL        = $(patsubst src/%.c,$(OBJDIR_REL)/%.o,$(SRCS))
ENGINE_OBJS_REL = $(patsubst src/%.c,$(OBJDIR_REL)/%.o,$(ENGINE_SRCS))

# Debug / Sanitized objects for tests
ENGINE_OBJS_DBG = $(patsubst src/%.c,$(OBJDIR_DBG)/%.o,$(ENGINE_SRCS))
TOURN_OBJS_DBG  = $(patsubst src/%.c,$(OBJDIR_DBG)/%.o,$(TOURN_SRCS))
OBJS_DBG        = $(patsubst src/%.c,$(OBJDIR_DBG)/%.o,$(SRCS))

TEST_MODULES = core moves scoring ai tournament
TEST_OBJS_DBG = $(patsubst %,$(OBJDIR_DBG)/test_%.o,$(TEST_MODULES))

.PHONY: all clean cte test run-test tests check test-bitboard run-test-bitboard bench run-bench install uninstall \
        test-core test-moves test-scoring test-ai test-tournament

all: cte test test-bitboard bench

dirs:
	@mkdir -p build $(OBJDIR_REL) $(OBJDIR_DBG)

# Pattern rule for release objects
$(OBJDIR_REL)/%.o: src/%.c $(HDRS) | dirs
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -c $< -o $@

# Pattern rule for debug / sanitized objects
$(OBJDIR_DBG)/%.o: src/%.c $(HDRS) | dirs
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -c $< -o $@

# Pattern rule for test module objects (compiled for library linkage)
$(OBJDIR_DBG)/test_%.o: test/test_%.c test/test_common.h $(HDRS) | dirs
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -I./test -c $< -o $@

# Pattern rule for standalone test module objects
$(OBJDIR_DBG)/standalone_test_%.o: test/test_%.c test/test_common.h $(HDRS) | dirs
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -I./test -DTEST_STANDALONE -c $< -o $@

# Main cte binary (optimized release build)
cte: build/cte

build/cte: $(OBJS_REL) $(OBJDIR_REL)/main.o
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o $@ $^ $(LDLIBS)

$(OBJDIR_REL)/main.o: main.c $(HDRS) | dirs
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -c $< -o $@

# Master test binary (combines all modules)
test: build/test
	./build/test

build/test: $(OBJS_DBG) $(TEST_OBJS_DBG) $(OBJDIR_DBG)/test_main.o
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $^ $(LDLIBS)

$(OBJDIR_DBG)/test_main.o: test/test_main.c test/test_common.h $(HDRS) | dirs
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -I./test -c $< -o $@

# Granular, lightning-fast standalone test targets (zero ncurses, < 0.2s rebuild)
test-ai: build/test_ai
	./build/test_ai

build/test_ai: $(ENGINE_OBJS_DBG) $(OBJDIR_DBG)/standalone_test_ai.o | dirs
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $^ -lm

test-moves: build/test_moves
	./build/test_moves

build/test_moves: $(ENGINE_OBJS_DBG) $(OBJDIR_DBG)/standalone_test_moves.o | dirs
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $^ -lm

test-core: build/test_core
	./build/test_core

build/test_core: $(ENGINE_OBJS_DBG) $(OBJDIR_DBG)/standalone_test_core.o | dirs
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $^ -lm

test-scoring: build/test_scoring
	./build/test_scoring

build/test_scoring: $(ENGINE_OBJS_DBG) $(OBJDIR_DBG)/standalone_test_scoring.o | dirs
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $^ -lm

test-tournament: build/test_tournament
	./build/test_tournament

build/test_tournament: $(TOURN_OBJS_DBG) $(OBJDIR_DBG)/standalone_test_tournament.o | dirs
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $^ -lm


# Differential bitboard tests
test-bitboard: build/test_bitboard
	./build/test_bitboard

build/test_bitboard: $(ENGINE_OBJS_DBG) test/test_bitboard.c $(HDRS) | dirs
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $(ENGINE_OBJS_DBG) test/test_bitboard.c -lm

# Benchmarks
bench: build/bench_backends

build/bench_backends: $(ENGINE_OBJS_REL) tools/bench_backends.c $(HDRS) | dirs
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o $@ $(ENGINE_OBJS_REL) tools/bench_backends.c -lm

TOURN_OBJS_REL = $(patsubst src/%.c,$(OBJDIR_REL)/%.o,$(TOURN_SRCS))

bench-cheater: build/bench_cheater
	./build/bench_cheater

build/bench_cheater: $(TOURN_OBJS_REL) tools/bench_cheater.c $(HDRS) | dirs
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o $@ $(TOURN_OBJS_REL) tools/bench_cheater.c -lm

run-test: test
run-test-bitboard: test-bitboard
run-bench: bench
	./build/bench_backends

tests: test test-bitboard
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
