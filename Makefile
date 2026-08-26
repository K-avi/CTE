CC ?= gcc
CFLAGS = -std=gnu2x -Wall -Wextra -Wpedantic -Wno-unused-parameter -march=native -I./include
RELEASE_FLAGS = -O3 -funroll-loops
DEBUG_FLAGS = -g -Og -fsanitize=address -fsanitize=undefined

SRCS = src/card.c src/player.c src/move.c src/game.c src/eval.c src/minmax.c src/front_cli.c
HDRS = include/card.h include/player.h include/move.h include/game.h include/eval.h include/minmax.h include/front_cli.h include/cte.h

.PHONY: all clean cte test run-test tests check

all: cte test

build:
	@mkdir -p build

cte: build $(SRCS) $(HDRS) main.c
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o build/cte $(SRCS) main.c

test: build $(SRCS) $(HDRS) test/test.c
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o build/test $(SRCS) test/test.c

run-test: test
	./build/test

tests: run-test
check: run-test

clean:
	rm -rf build
