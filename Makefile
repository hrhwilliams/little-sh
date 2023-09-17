CC := gcc
CFLAGS := -Wall -pedantic -g

lsh: main.c
	$(CC) $(CFLAGS) $^ -o $@

