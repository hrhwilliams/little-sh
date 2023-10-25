#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

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

void job() {

}

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
    RI_READ_FILE,         /* cmd  < file */
    RI_WRITE_FILE,        /* cmd  > file */
    RI_WRITE_APPEND_FILE, /* cmd >> file */
    RI_READ_WRITE_FILE,   /* cmd <> file */
    RI_REDIRECT_FD        /* cmd $1 >& $2*/
} RedirectInstruction;

/*
 linked-list of redirects for the command
*/
typedef struct _Redirect {
    struct _Redirect *next;
    RedirectInstruction instr;
    union {
        int fds[2];
        char *fp;
    };
} Redirect;

typedef struct _Command {
    Redirect *redirects;
    char **argv;
    int argc;
    int flags; 
} Command;

typedef struct _Pipeline {
    Command *commands;
    int count;
} Pipeline;

typedef enum {
    COND_NONE,
    COND_AND,
    COND_OR
} ConditionalType;

pid_t child_stack[128];
size_t child_stack_head = 0;

size_t push_async_child(pid_t child_pid) {
    child_stack[child_stack_head] = child_pid;
    return child_stack_head++;
}

void child_handler(int sig) {
    pid_t child_pid;
    int status;

    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // remove child_pid from stack
        printf("%d done\n", child_pid);
    }
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

int execute_builtin(char **argv, int argc, int *status) {
    switch (argv[0][0]) {
    case 'c': // cd, clear
        if (strcmp(argv[0], "cd") == 0 && argc == 2) {
            chdir(argv[1]);
            *status = 0;
            return 1;
        } if (strcmp(argv[0], "clear") == 0) {
            fprintf(stdout, "\033[2J");
            *status = 0;
            return 1;
        }
    case 'e': // export, echo, exit
        if (strcmp(argv[0], "export") == 0 && argc == 2) {
            *status = setenv(argv[0], argv[1], 1);
            return 1;
        } if (strcmp(argv[0], "echo") == 0 && argc > 1) {
            for (int i = 1; i < argc && argv[i]; i++) {
                fprintf(stdout, "%s ", argv[i]);
            }
            fprintf(stdout, "\n");
            *status = 0;
            return 1;
        } if (strcmp(argv[0], "exit") == 0 && argc == 3) {
            *status = 0;
            return 1;
        }
    case 'j': // jobs
        if (strcmp(argv[0], "jobs") == 0) {
            *status = 0;
            return 1;
        }
    case 'k': // kill
        if (strcmp(argv[0], "kill") == 0 && argc == 3) {
            *status = kill(atoi(argv[2]), atoi(argv[1]));
            return 1;
        }
    case 'p': // pwd
        if (strcmp(argv[0], "pwd") == 0) {
            fprintf(stdout, "%s\n", builtin_pwd());
            *status = 0;
            return 1;
        }
    case 'q':
        if (strcmp(argv[0], "quit") == 0) {
            exit(0);
        }
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
        if (execute_builtin(command->argv, command->argc, &builtin_status)) {
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
            printf("[%lu] %d\n", child_stack_id, pid);
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

    for (size_t i = 0; i < p->count; i++) {
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

        run_command(&p->commands[i], pipe_in, pipe_out, 1, &return_value);
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
    }
}

char* builtin_pwd() {
    static char pwd_buf[PATH_MAX];
    getcwd(pwd_buf, sizeof pwd_buf);
    return pwd_buf;
}

void run_interactive() {
    char dir_buffer[PATH_MAX];
    char *line = NULL;
    size_t bytes = 0;

    getcwd(dir_buffer, sizeof dir_buffer);

    for (; !feof(stdin);) {
        printf("%s$ ", dir_buffer);
        getline(&line, &bytes, stdin);
    }

    free(line);
}

void pipeline1() {
    // execute echo "hello, world" | wc -c > test.txt
    char *command1[3];
    char *command2[3];

    command1[0] = "echo";
    command1[1] = "hello world";
    command1[2] = NULL;

    command2[0] = "wc";
    command2[1] = "-c";
    command2[2] = NULL;

    Redirect re;
    re.next = NULL;
    re.instr = RI_WRITE_FILE;
    re.fp = "test.txt";

    Command commands[3];
    commands[0].redirects = NULL;
    commands[0].argv = command1;
    commands[0].argc = 2;

    commands[1].redirects = &re;
    commands[1].argv = command2;
    commands[1].argc = 2;

    Pipeline p;
    p.commands = commands;
    p.count = 2;

    run_pipeline(&p);
}

void pipeline2() {
    // execute pwd > test2.txt
    char *argv[2];
    argv[0] = "pwd";
    argv[1] = NULL;

    Redirect re;
    re.next = NULL;
    re.instr = RI_WRITE_FILE;
    re.fp = "test2.txt";

    Command command;
    command.argv = argv;
    command.argc = 1;
    command.redirects = &re;

    Pipeline p;
    p.commands = &command;
    p.count = 1;

    run_pipeline(&p);
}

void pipeline3() {
    // execute grep -Tn "define" /usr/include/linux/sched.h > test3.txt | sort | wc -l > test4.txt > test5.txt
    char *argv1[10] = { "echo", "hello,", "world", "how", "art", "thou", "doing", "today", ":D", NULL };
    char *argv2[2] = { "cat", NULL };
    char *argv3[2] = { "cat", NULL };
    char *argv4[2] = { "cat", NULL };
    char *argv5[2] = { "cat", NULL };
    char *argv6[2] = { "cat", NULL };

    Command commands[6];
    commands[0].argv = argv1;
    commands[0].argc = 10;
    commands[0].redirects = NULL;

    commands[1].argv = argv2;
    commands[1].argc = 2;
    commands[1].redirects = NULL;

    commands[2].argv = argv3;
    commands[2].argc = 2;
    commands[2].redirects = NULL;

    commands[3].argv = argv4;
    commands[3].argc = 2;
    commands[3].redirects = NULL;

    commands[4].argv = argv5;
    commands[4].argc = 2;
    commands[4].redirects = NULL;

    Redirect redirect;
    redirect.fp = "cat.txt";
    redirect.instr = RI_WRITE_APPEND_FILE;
    redirect.next = NULL;

    commands[5].argv = argv6;
    commands[5].argc = 2;
    commands[5].redirects = &redirect;

    Pipeline p;
    p.commands = commands;
    p.count = 6;

    run_pipeline(&p);
}

void pipeline4() {
    // execute pwd > test2.txt
    char *argv[3];
    argv[0] = "sleep";
    argv[1] = "1";
    argv[2] = NULL;

    Command command;
    command.argv = argv;
    command.argc = 2;
    command.redirects = NULL;

    Pipeline p;
    p.commands = &command;
    p.count = 1;

    run_pipeline(&p);
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = child_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    // pipeline1();
    // pipeline2();
    // pipeline3();
    pipeline4();

    for (;;);
}
