#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>

#include <readline/readline.h>
#include <readline/history.h>
#include <glob.h>

#include "quash.h"
#include "arrays.h"
#include "tokenizer.h"
#include "parser.h"

/* ----------------------------- */
/*          jobs stack           */
/* ----------------------------- */

#define STACK_SIZE 256

enum JobFlags {
    JOB_RUNNING = 0x01,
    JOB_SUSPENDED = 0x02,
    JOB_ASYNC = 0x04,
};

typedef struct _Job {
    pid_t pid;
    int flags;
    char *line;
} Job;

struct {
    int indices[STACK_SIZE];
    Job jobs[STACK_SIZE];
} job_stack;

void init_job_stack() {
    /* from now on we just ignore the first entry so job_ids map one-to-one with indices in the stack */
    for (size_t n = 0; n < STACK_SIZE; n++) {
        job_stack.indices[n] = n;
        job_stack.jobs[n].pid = 0;
        job_stack.jobs[n].flags = 0;
        job_stack.jobs[n].line = NULL;
    }
}

/* return the lowest available index */
int next_job_index() {
    /* find first nonzero entry in `indices`, zero it out, and return it */
    for (size_t n = 1; n < STACK_SIZE; n++) {
        if (job_stack.indices[n] != 0) {
            int index = job_stack.indices[n];
            job_stack.indices[n] = 0;
            return index;
        }
    }

    return -1; /* stack is full */
}

/* try to find the index of the job with process id `pid` */
int find_job_index(pid_t pid) {
    for (size_t n = 1; n < STACK_SIZE; n++) {
        if (job_stack.indices[n] == 0 && job_stack.jobs[n].pid == pid) {
            return n;
        }
    }

    return -1; /* pid not in the list of jobs */
}

Job *get_job(int n) {
    return &(job_stack.jobs[n]);
}

void return_job_index(int n) {
    /* restore that entry in `indices` */
    if (job_stack.indices[n] == 0) {
        job_stack.indices[n] = n;
        job_stack.jobs[n].pid = 0;
        job_stack.jobs[n].flags = 0;

        free(job_stack.jobs[n].line);
    }
}

int push_job(pid_t pid, int flags) {
    int index = next_job_index();
    job_stack.jobs[index].pid = pid;
    job_stack.jobs[index].flags = flags;
    return index;
}

void add_flag(int n, int flag) {
    if (job_stack.indices[n] == 0) {
        job_stack.jobs[n].flags |= flag;
    }
}

/* --------------------------------------- */
/*             signal handlers             */
/* --------------------------------------- */

/* jump buffer to return to if a running job is suspended */
jmp_buf from_suspended;

struct sigaction old_sigint;
struct sigaction old_sigtstp;
struct sigaction old_sigchld;
struct sigaction tstp_sigaction;

void sigchld_handler() {
    pid_t child_pid;
    int status;

    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int job_id = find_job_index(child_pid);

        printf("\n[%d] %d done\t%s\n", job_id, child_pid, get_job(job_id)->line);
        return_job_index(job_id);

        /* have readline go to a new line */
        rl_on_new_line();
        rl_redisplay();
    }
}

void sigtstp_ignorer() {
    /* nothing */
}

void sigtstp_handler() {
    longjmp(from_suspended, 1);
}

void sigint_ignorer() {
    /* nothing */
}

void set_tstp_longjump_handler() {
    tstp_sigaction.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &tstp_sigaction, NULL);
}

void ignore_tstp() {
    tstp_sigaction.sa_handler = sigtstp_ignorer;
    sigaction(SIGTSTP, &tstp_sigaction, NULL);
}

void restore_signal_handlers() {
    sigaction(SIGINT, &old_sigint, NULL);
    sigaction(SIGTSTP, &old_sigtstp, NULL);
}

void init_signal_handlers() {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, &old_sigchld);

    sa.sa_handler = sigint_ignorer;
    sigaction(SIGINT, &sa, &old_sigint);

    sa.sa_handler = sigtstp_ignorer;
    sigaction(SIGTSTP, &sa, &old_sigtstp);

    tstp_sigaction = sa;
}


/*
1. expand variables
2. expand globs
3. convert everything into tokens
4. parse shell grammar with those tokens
5. evaluate parsed $command
6. repeat
*/

/* ----------------------------- */
/*        shell functions        */
/* ----------------------------- */

char* builtin_pwd() {
    static char pwd_buf[PATH_MAX];
    getcwd(pwd_buf, sizeof pwd_buf);
    return pwd_buf;
}

void print_history() {
    HIST_ENTRY **entry = history_list();
    for (int i = 0; entry[i] && i < history_length; i++) {
        fprintf(stdout, "%-6d %s\n", i, entry[i]->line);
    }
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
        if (strcmp(argv[0], "export") == 0 && argc == 2 && argv[1]) {
            *status = putenv(strdup(argv[1]));
            return 1;
        } if (strcmp(argv[0], "exit") == 0) {
            *status = 0;
            exit(0);
            return 1;
        }
        break;
    case 'j': // jobs
        if (strcmp(argv[0], "jobs") == 0) {
            for (int i = 1; i < STACK_SIZE; i++) {
                if (job_stack.indices[i] == 0) {
                    printf("[%d] %d\n", i, job_stack.jobs[i].pid);
                }
            }
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

void run_redirects(Redirect *redirect) {
    while (redirect != NULL) {
        FILE *f;

        switch (redirect->instr) {
        case RI_READ_FILE:
            if ((f = fopen(redirect->fp, "r")) != NULL) {
                dup2(fileno(f), STDIN_FILENO);
            } else {
                perror("fopen");
            }
            break;
        case RI_WRITE_FILE:
            if ((f = fopen(redirect->fp, "w")) != NULL) {
                dup2(fileno(f), STDOUT_FILENO);
            } else {
                perror("fopen");
            }
            break;
        case RI_WRITE_APPEND_FILE:
            if ((f = fopen(redirect->fp, "a")) != NULL) {
                dup2(fileno(f), STDOUT_FILENO);
            } else {
                perror("fopen");
            }
            break;
        case RI_READ_WRITE_FILE:
            if ((f = fopen(redirect->fp, "w+")) != NULL) {
                dup2(fileno(f), STDIN_FILENO);
                dup2(fileno(f), STDOUT_FILENO);
            } else {
                perror("fopen");
            }
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
    volatile pid_t pid;
    int status;

    if (execute_builtin(command, return_value)) {
        return;
    }

    if (setjmp(from_suspended)) {
        /* child was suspended */
        printf("%d was suspended\n", pid);
        return;
    }

    if ((pid = fork()) == -1) {
        perror("fork");
    } else if (pid == 0) {
        restore_signal_handlers();

        if (pipe_in != -1) {
            dup2(pipe_in, STDIN_FILENO);
            close(pipe_in);
        }
        if (pipe_out != -1) {
            dup2(pipe_out, STDOUT_FILENO);
            close(pipe_out);
        }

        run_redirects(command->redirects);

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
        int job_index = push_job(pid, 0);

        if (asynchronous) {
            printf("[%d] %d\n", job_index, pid);
            add_flag(job_index, JOB_ASYNC);
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

            return_job_index(job_index);
        }
    }
}

void run_pipeline(Pipeline *p) {
    int return_value;
    int fds[p->count][2];

    Command *command = p->commands;

    set_tstp_longjump_handler();

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

    ignore_tstp();
    // putenv("?=%d", return_value)
}


/* ----------------------------- */
/*         main function         */
/* ----------------------------- */

void newline() {
    puts("");
}

int interactive_prompt() {
    const char *prompt = "$ ";
    char *line;

    for (;;) {
        TokenDynamicArray tokens;
        
        line = readline(prompt);

        if (line == NULL) {
            newline();
            break;
        } else if (line[0] == '\0') {
            continue;
        }

        add_history(line);
        create_token_array(&tokens);

        if (!tokenize(&tokens, line)) {
            free(line);
            free_token_array(&tokens);
            continue;
        }

        Pipeline *p = parse(&tokens);
        run_pipeline(p);
        free_pipeline(p);

        for (size_t i = 0; i < tokens.length; i++) {
            printf("('%s' : %d), ", tokens.tuples[i].text, tokens.tuples[i].token);
        }
        printf("\b \n");
#if 0

        Pipeline *pipeline = eval(&tokens);
        run_pipeline(pipeline);
#endif
        free(line);
        free_token_array(&tokens);
    }

    return 0;
}

int main() {
    init_job_stack();
    init_signal_handlers();

    interactive_prompt();

    return 0;
}
