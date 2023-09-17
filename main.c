#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#define PROMPT "$ "

char **parse_input_argv(char *input) {
    int input_argv_slots = 2;
    char **input_argv = malloc(input_argv_slots * sizeof *input_argv);

    const char *delim = " \t\n";
    char *token = strtok(input, delim);

    int i = 0;

    do {
        if (i == input_argv_slots - 1) {
            input_argv_slots *= 2;
            input_argv = realloc(input_argv, input_argv_slots);
        }
        input_argv[i] = strdup(token);

        i++;
    } while ((token = strtok(NULL, delim)) != NULL);

    input_argv[i] = NULL;
    return input_argv;
}

char **get_input() {
    static char *buffer = NULL;
    static size_t bytes_read = 0;

    printf(PROMPT);
    fflush(stdout);
    if (getline(&buffer, &bytes_read, stdin) == -1 && errno != 0) {
        perror("getline");
        return NULL;
    }

    if (feof(stdin)) {
        return NULL;
    }

    return parse_input_argv(buffer);
}

int do_builtin(char **input_argv) {
    static char pwdbuf[256];

    if (input_argv == NULL || input_argv[0] == NULL) {
        return 0;
    }

    switch (input_argv[0][0]) {
    case 'c':
        if (strncmp(input_argv[0], "cd", 2) == 0) {
            if (chdir(input_argv[1]) == -1) {
                perror("cd");
            }
            return 1;
        } else if (strncmp(input_argv[0], "clear", 5) == 0) {
            printf("\033[2J\033[H");
            return 1;
        }
        return 0;
    case 'p':
        if (strncmp(input_argv[0], "pwd", 3) == 0) {
            if (getcwd(pwdbuf, sizeof pwdbuf) == NULL) {
                perror("getcwd");
                return 0;
            }
            printf("%s\n", pwdbuf);
            return 1;
        }
        return 0;
    case 'e':
        if (strncmp(input_argv[0], "exit", 4) == 0) {
            exit(0);
        }
        return 0;
    default:
        return 0;
    }
}

void run_command(char **input_argv) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        if (execvp(input_argv[0], input_argv) < 0) {
            perror("exec");
        }

        exit(1);
    } else if (pid == -1) {
        perror("fork");
    } else {
        wait(&status);
    }
}

void run_interactive() {
    for (;;) {
        char **input_argv = get_input();

        if (input_argv == NULL) {
            printf("\n");
            break;
        }
        
        if (do_builtin(input_argv)) {
            continue;
        }

        run_command(input_argv);
    }
}

int main(int argc, char *argv[]) {
    run_interactive();
    return 0;
}
