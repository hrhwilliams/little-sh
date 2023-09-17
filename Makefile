CC := gcc
CFLAGS := -Wall -pedantic
OFLAGS := -O1

lsh: main.c
	$(CC) $(CFLAGS) $(OFLAGS) $^ -o $@

