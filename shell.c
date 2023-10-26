#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "parser.h"
#include "shell.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/* conditional := job ('&&' | '||') conditional 
 *              | job '&'?
 * job := command ('>' | '2>' output)? ('|' job)* 
 * command := word (' ' word)
 * word := non-escape-char-or-metachar character+
 * metachar := ' ', '\t', '\n', '|', '&', ';', '(', ')', '<', or '>'
 */

#ifdef _DEBUG
#define PERROR2(x, cmp) \
({ __typeof__ (x) _x = (x); \
    if (_x == cmp) { \
        perror(#x); \
        fflush(stderr); \
        exit(1); \
    } \
_x; })
#define PERROR(x) PERROR2(x, -1)
#else
#define PERROR(x) (x)
#define PERROR2(x, cmp) (x)
#endif

typedef enum {
    COND_NONE,
    COND_AND,
    COND_OR
} ConditionalType;

#define STACK_SIZE 128

pid_t child_stack[STACK_SIZE];
HIST_ENTRY *child_cmd_stack[STACK_SIZE];
int child_executing_stack[STACK_SIZE];
int child_stack_head = 0;

size_t push_async_child(pid_t child_pid) {
    child_stack[child_stack_head] = child_pid;
    child_cmd_stack[child_stack_head] = current_history();
    child_executing_stack[child_stack_head] = 1;
    return child_stack_head++;
}

void sigchld_handler() {
    pid_t child_pid;
    int status;

    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = child_stack_head; i > -1; i--) {
            if (child_pid == child_stack[i]) {
                printf("\n[%d] %d done\t%s\n", i + 1, child_pid, child_cmd_stack[i]->line);
                child_executing_stack[i] = 0;

                rl_on_new_line();
                rl_redisplay();
            }
        }
    }
}

void sigtstp_handler() {
    // puts("");
    // rl_on_new_line();
    // rl_redisplay();
}

void sigint_handler() {
    // puts("");
    // rl_on_new_line();
    // rl_redisplay();
}

void init_sigchld_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);

    memset(child_stack, 0, STACK_SIZE * sizeof *child_stack);
    memset(child_cmd_stack, 0, STACK_SIZE * sizeof *child_cmd_stack);
    memset(child_executing_stack, 0, STACK_SIZE * sizeof *child_executing_stack);
}

/*
 Binary tree: right side executes conditionally depending on the return type
 of the left side. If the conditional is a `COND_AND`, it executes the right
 side when the left side returns 0 in a success. Otherwise if the conditional
 is a `COND_OR`, the right side only executes if the left side returns nonzero
 in an error.

 If both `left` and `right` are NULL and cond is `COND_NONE`, run `pipeline`;
 else, run `left`, look at its return value, and conditionally run `right`.
*/
typedef struct _Conditional {
    struct _Conditional *left;
    struct _Conditional *right;
    Pipeline *pipeline;
    ConditionalType cond;
} Conditional;

char* builtin_pwd();

void print_history() {
    #ifdef __linux__
    HIST_ENTRY **entry = history_list();
    for (int i = 0; entry[i] && i < history_length; i++) {
        fprintf(stdout, "%-6d %s\n", i, entry[i]->line);
    }
    #else
    for (int i = 0; i < history_length; i++) {
        HIST_ENTRY *entry = history_get(i);
        fprintf(stdout, "%-6d %s\n", i, entry->line);
    }
    #endif
}

int execute_builtin(Command *command, int *status) {
    char **argv = command->argv;
    int argc = command->argc;
    switch (argv[0][0]) {
    case 'c': // cd, clear
        if (strcmp(argv[0], "cd") == 0 && argc == 2) {
            chdir(argv[1]);
            setenv("PWD", builtin_pwd(), 1);
            *status = 0;
            return 1;
        } if (strcmp(argv[0], "clear") == 0) {
            fprintf(stdout, "\033[2J");
            *status = 0;
            return 1;
        }
        break;
    case 'e': // export, echo, exit
        if (strcmp(argv[0], "export") == 0 && argc == 2) {
            *status = putenv(argv[1]);
            return 1;
        } if (strcmp(argv[0], "exit") == 0) {
            *status = 0;
            exit(0);
            return 1;
        }
        break;
    case 'j': // jobs
        if (strcmp(argv[0], "jobs") == 0) {
            *status = 0;
            return 1;
        }
        break;
    case 'k': // kill
        if (strcmp(argv[0], "kill") == 0 && argc == 3) {
            *status = kill(atoi(argv[2]), atoi(argv[1]));
            return 1;
        }
        break;
    case 'p': // pwd
        if (strcmp(argv[0], "pwd") == 0) {
            fprintf(stdout, "%s\n", builtin_pwd());
            *status = 0;
            return 1;
        }
        break;
    case 'q':
        if (strcmp(argv[0], "quit") == 0) {
            exit(0);
        }
        break;
    default:
        break;
    }

    return 0;
}

int execute_forkable_builtin(Command *command, int *status) {
    char **argv = command->argv;
    int argc = command->argc;
    switch (argv[0][0]) {
    case 'e': // export, echo, exit
        if (strcmp(argv[0], "echo") == 0 && argc > 1) {
            for (int i = 1; i < argc && argv[i]; i++) {
                fprintf(stdout, "%s ", argv[i]);
            }
            fprintf(stdout, "\n");
            *status = 0;
            return 1;
        }
        break;
    case 'h': // history
        if (strcmp(argv[0], "history") == 0) {
            print_history();
            *status = 0;
            return 1;
        }
        break;
    case 'p': // pwd
        if (strcmp(argv[0], "pwd") == 0) {
            fprintf(stdout, "%s\n", builtin_pwd());
            *status = 0;
            return 1;
        }
        break;
    default:
        break;
    }

    return 0;
}

void eval_redirects(Redirect *redirect) {
    while (redirect != NULL) {
        FILE *f;

        switch (redirect->instr) {
        case RI_READ_FILE:
            f = PERROR2(fopen(redirect->fp, "r"), NULL);
            PERROR(dup2(fileno(f), STDIN_FILENO));
            break;
        case RI_WRITE_FILE:
            f = PERROR2(fopen(redirect->fp, "w"), NULL);
            PERROR(dup2(fileno(f), STDOUT_FILENO));
            break;
        case RI_WRITE_APPEND_FILE:
            f = PERROR2(fopen(redirect->fp, "a"), NULL);
            PERROR(dup2(fileno(f), STDOUT_FILENO));
            break;
        case RI_READ_WRITE_FILE:
            f = PERROR2(fopen(redirect->fp, "r+"), NULL);
            PERROR(dup2(fileno(f), STDIN_FILENO));
            PERROR(dup2(fileno(f), STDOUT_FILENO));
            break;
        case RI_REDIRECT_FD:
            dup2(redirect->fds[0], redirect->fds[1]);
            break;
        default:
            break;
        }

        redirect = redirect->next;
    }
}

void run_command(Command *command, int pipe_in, int pipe_out, int asynchronous, int *return_value) {
    pid_t pid;
    int status;

    if (execute_builtin(command, return_value)) {
        return;
    }

    if ((pid = fork()) == -1) {
        perror("fork");
    } else if (pid == 0) {
        if (pipe_in != -1) {
            dup2(pipe_in, STDIN_FILENO);
            close(pipe_in);
        }
        if (pipe_out != -1) {
            dup2(pipe_out, STDOUT_FILENO);
            close(pipe_out);
        }

        eval_redirects(command->redirects);

        int builtin_status;
        if (execute_forkable_builtin(command, &builtin_status)) {
            if (builtin_status == -1) {
                perror(command->argv[0]);
            }

            exit(builtin_status);
        } else if (execvp(command->argv[0], command->argv) == -1) {
            perror(command->argv[0]);
        }

        exit(-1);
    } else {
        if (asynchronous) {
            // push child onto async process stack
            size_t child_stack_id = push_async_child(pid);
            printf("[%lu] %d\n", child_stack_id + 1, pid);
        } else {
            if (waitpid(pid, &status, 0) == -1) {
                perror("waitpid");
            }

            if (WIFEXITED(status)) {
                *return_value = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                *return_value = WTERMSIG(status);
                printf("%d - %s\n", *return_value, strsignal(*return_value));
            } else {
                *return_value = -1;
            }
        }
    }
}

void run_pipeline(Pipeline *p) {
    int return_value;
    int fds[p->count][2];

    Command *command = p->commands;

    for (int i = 0; command && i < p->count; i++) {
        pipe(fds[i]);
        int pipe_in, pipe_out;

        if (i == 0) {
            pipe_in = -1;
        } else {
            pipe_in = fds[i-1][0];
        }

        if (i == p->count - 1) {
            pipe_out = -1;
        } else {
            pipe_out = fds[i][1];
        }

        run_command(command, pipe_in, pipe_out, p->asynchronous, &return_value);
        if (return_value != 0) {
            // break!
        }

        // close unneeded fds
        if (i > 0) {
            close(pipe_in);
        }

        if (i < p->count - 1) {
            close(pipe_out);
        }

        command = command->next;
    }
    
    // putenv("?=%d", return_value)
}

char* builtin_pwd() {
    static char pwd_buf[PATH_MAX];
    getcwd(pwd_buf, sizeof pwd_buf);
    return pwd_buf;
}
