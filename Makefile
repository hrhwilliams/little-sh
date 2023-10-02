CC := gcc
CFLAGS := -Wall -Wextra -g

lsh: main.c
	$(CC) $(CFLAGS) $^ -o $@

