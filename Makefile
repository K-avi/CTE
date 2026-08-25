CC ?= gcc
CFLAGS = -std=gnu2x -Wall -Wextra -Wpedantic -Wno-unused-parameter -march=native -I./include
RELEASE_FLAGS = -O3 -funroll-loops
DEBUG_FLAGS = -g -Og -fsanitize=address -fsanitize=undefined

.PHONY: all clean cte test run-test

all: cte test

build:
	@mkdir -p build

cte: build
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o build/cte src/cte.c main.c

test: build
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o build/test test/test.c src/cte.c

run-test: test
	./build/test

clean:
	rm -rf build

