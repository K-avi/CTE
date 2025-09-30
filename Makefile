CC=gcc
CFLAGS=-O2 -std=gnu2x -Wall  -Wextra -Wpedantic -Wno-unused-parameter

all: cte

cte : 
	$(CC) $(CFLAGS) -o $@ $^ main.c

clean: rm cte

