#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define PROMPT "$ "

char cwd[512];

void handle_sigint() {

}

char **parse_input_argv(char *input) {
    int input_argv_slots = 2;
    char **input_argv = malloc(input_argv_slots * sizeof *input_argv);

    const char *delim = " \t\n";
    char *token = strtok(input, delim);

    int i = 0;
    while (token != NULL) {
        if (i == input_argv_slots - 1) {
            input_argv_slots *= 2;
            input_argv = realloc(input_argv, input_argv_slots * sizeof *input_argv);
        }

        if (token[0] == '$') {
            char *env_var = getenv(token + 1);
            if (env_var) {
                input_argv[i] = strdup(env_var);
            } else {
                input_argv[i] = strdup("");
            }
        } else {
            input_argv[i] = strdup(token);
        }

        i++;
        token = strtok(NULL, delim);
    }

    input_argv[i] = NULL;
    return input_argv;
}

char **get_input() {
    static char *buffer = NULL;
    static size_t bytes_read = 0;

    if (getline(&buffer, &bytes_read, stdin) == -1 && errno != 0) {
        if (errno == EINTR) {
            return NULL;
        }
        perror("getline");
        return NULL;
    }

    if (feof(stdin)) {
        return NULL;
    }

    return parse_input_argv(buffer);
}

void get_cwd() {
    if (getcwd(cwd, sizeof cwd) == NULL) {
        perror("getcwd");
    }
}

void free_input_argv(char **input_argv) {
    if (input_argv == NULL) {
        return;
    }

    for (int i = 0; input_argv[i] != NULL; i++) {
        free(input_argv[i]);
    }

    free(input_argv);
}

int builtin_cd(char *dest) {
    if (chdir(dest) == -1) {
        perror("cd");
    }

    get_cwd();

    return 1;
}

int builtin_clear() {
    printf("\033[2J\033[H");
    return 1;
}

int builtin_pwd() {
    static char pwdbuf[256];
    if (getcwd(pwdbuf, sizeof pwdbuf) == NULL) {
        perror("getcwd");
    }

    printf("%s\n", pwdbuf);
    return 1;
}

int do_builtin(char **input_argv) {
    if (input_argv == NULL || input_argv[0] == NULL) {
        return 0;
    }

    switch (input_argv[0][0]) {
    case 'c':
        if (strncmp(input_argv[0], "cd", 2) == 0) {
            return builtin_cd(input_argv[1]);
        } else if (strncmp(input_argv[0], "clear", 5) == 0) {
            return builtin_clear();
        }
        return 0;
    case 'p':
        if (strncmp(input_argv[0], "pwd", 3) == 0) {
            return builtin_pwd();
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
        /* clear errno to fix getline reporting the errors of previous commands */
        errno = 0;
        printf("%s%s", cwd, PROMPT);
        fflush(stdout);

        char **input_argv = get_input();

        if (input_argv == NULL) {
            printf("\n");
        }
        
        if (do_builtin(input_argv)) {
            continue;
        }

        run_command(input_argv);
        free_input_argv(input_argv);
    }
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = &handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    get_cwd();

    run_interactive();

    return 0;
}
