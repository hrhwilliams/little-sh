CC := gcc
CFLAGS := -O2 -march=native -flto
WARNS := -Wall -Wextra -pedantic -Wno-strict-prototypes
DEBUG := -g # -fsanitize=address
OUTFILE := qsh

release: arrays.c quash.c tokenizer.c parser.c jobs.c hash.c
	$(CC) $^ $(CFLAGS) -lreadline  -o $(OUTFILE)

debug: arrays.c quash.c tokenizer.c parser.c jobs.c hash.c
	$(CC) $^ $(WARNS) $(DEBUG) -lreadline  -o $(OUTFILE)-debug

test: $(OUTFILE)-debug
	$(CC) $(WARNS) $(DEBUG) test.c -lreadline -o $@
