CC := gcc
CFLAGS := -O2
WARNS := -Wall -Wextra -pedantic -Wno-strict-prototypes
DEBUG := -g #-fsanitize=address

quash: arrays.c quash.c tokenizer.c parser.c jobs.c
	$(CC) $^ $(WARNS) $(DEBUG) -lreadline  -o $@
