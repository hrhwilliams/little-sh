CC := gcc

qsh: quash.c
	$(CC) quash.c -Wall -g -lreadline -o $@

