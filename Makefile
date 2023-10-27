CC := gcc
CFLAGS := -O2
WARNS := -Wall -Wextra
DEBUG := -g # -fsanitize=address

qsh: quash.c tokenizer.c string_buf.c parser.c shell.c
	$(CC) $^ $(WARNS) $(DEBUG) -lreadline  -o $@

fuzzer: fuzzer.c tokenizer.c string_buf.c
	clang $^ -fsanitize=fuzzer -o $@
