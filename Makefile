CC := gcc
CFLAGS := -O2
WARNS := -Wall -Wextra -pedantic
DEBUG := -g # -fsanitize=address

quash: arrays.c quash.c tokenizer.c parser.c
	$(CC) $^ $(WARNS) $(DEBUG) -lreadline  -o $@
