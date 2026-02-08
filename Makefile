CC=gcc
CFLAGS=-std=gnu2x -Wall  -Wextra -Wpedantic -Wno-unused-parameter -march=native -I./include
RELEASE_FLAGS=-O3 -funroll-loops 
DEBUG_FLAGS=-g -Og  #-fsanitize=address -fsanitize=undefined
VPATH=src:include:test

cte : 
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o build/cte src/cte.c main.c

test :
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o build/test test/test.c src/cte.c

clean :
	rm -rf build/*
