# Quash Shell

Quite a shell, done for EECS 678 Intro to Operating Systems. Build with `make quash` and run `./quash`. This includes GNU Readline to make input much nicer, though sometimes the library it might leak a byte or two during signal handling.

## Features

- essential
  - commands without arguments
  - commands with arguments
  - variable assignment and variable expansion in commands
  - `echo`, `export`, `cd`, `pwd`, `quit`, `exit`, 
  - `jobs`, `kill`
  - `>` redirect
  - `<` redirect
  - pipes
  - background processes
- extra credit
  - conditional AND (`&&`) and OR (`||`)
  - pipes, redirects, and conditionals eval in proper order
  - pipes and redirects work with built-in commands
  - `<>` redirect (redirect stdout and stdin to/from file)
  - `>>` redirect (redirect stdout to file, appending)
  - `>&` redirect (redirect stderr to file)
  - `>>&` redirect (redirect stderr to file, appending)
  - GNU readline & history
  - `~` expansion

## Examples

```
$ find . -name '*.c' | xargs wc
  375  1134  9610 ./tokenizer.c
  175   471  4078 ./parser.c
  691  1905 17437 ./quash.c
   33    91   706 ./hash_test.c
   89   236  2582 ./arrays.c
  257   656  5814 ./jobs.c
  125   268  2864 ./hash.c
 1745  4761 43091 total
```

```
$ export output_file=out.txt
$ find . -name '*.c' | xargs wc > $output_file && cat $output_file
  375  1134  9610 ./tokenizer.c
  175   471  4078 ./parser.c
  691  1905 17437 ./quash.c
   33    91   706 ./hash_test.c
   89   236  2582 ./arrays.c
  257   656  5814 ./jobs.c
  125   268  2864 ./hash.c
 1745  4761 43091 total
$ echo $output_file
out.txt
```

```
$ pwd
/home/harlan/programming/school/quash
$ mkdir test && cd test && pwd
/home/harlan/programming/school/quash/test
```

```
$ ls ~/programming/school/quash/
Makefile  README.md  TODO.md  arrays.c  arrays.h  hash.c  hash.h  hash_test.c  jobs.c  jobs.h  out.txt  parser.c  parser.h  quash  quash.c  quash.h  tokenizer.c  tokenizer.h
```

```
$ history
0      find . -name '*.c' | xargs wc
1      export output_file=out.txt
2      find . -name '*.c' | xargs wc > $output_file && cat $output_file
3      echo $output_file
4      pwd
5      mkdir test && cd test && pwd
6      ls ~/programming/school/quash/
7      history
```