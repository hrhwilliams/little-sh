#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>
#include <glob.h>

#include "quash.h"
#include "arrays.h"
#include "tokenizer.h"
#include "parser.h"
#include "jobs.h"

/* --------------------------------------- */
/*             signal handlers             */
/* --------------------------------------- */

/* jump buffer to return to if a running job is suspended */
jmp_buf from_suspended;
jmp_buf env;

struct sigaction old_sigint;
struct sigaction old_sigtstp;
struct sigaction old_sigchld;
struct sigaction tstp_sigaction;

void sigchld_handler() {
    pid_t child_pid;
    int status;
    int should_jump = 0;

    while ((child_pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        // int job_id = find_job_index(child_pid);

        if (status == SIGKILL || status == SIGTERM || WIFEXITED(status)) {
            Job *job = get_job_from_pid(child_pid);
            if (job && all_completed(job->id)) {
                printf("[%d] %d finished with exit status %d", job->id, child_pid, status);
                free_job(job->id);
            }
            should_jump = 1;
        } else if (WIFSTOPPED(status)) {
            printf("%d suspended", child_pid);
            should_jump = 1;
        }
    }

    if (should_jump) {
        siglongjmp(env, 1);
    }
}

void sigtstp_ignorer() {
    siglongjmp(env, 1);
}

void sigtstp_handler() {
    siglongjmp(from_suspended, 1);
}

void sigint_ignorer() {
    siglongjmp(env, 1);
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
    sigaction(SIGCHLD, &old_sigchld, NULL);
}

void init_signal_handlers() {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, &old_sigchld);

    sa.sa_flags = 0;
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
    #ifdef __APPLE__ /* apple uses a different readline library */
    for (int i = 0; i < history_length; i++) {
        fprintf(stdout, "%-6d %s\n", i, history_get(i)->line);
    }
    #else
    HIST_ENTRY **entry = history_list();
    for (int i = 0; entry[i] && i < history_length; i++) {
        fprintf(stdout, "%-6d %s\n", i, entry[i]->line);
    }
    #endif
}

int wait_job(pid_t pid) {
    int status;
    /* returns if job finishes or is suspended */
    if (waitpid(pid, &status, WUNTRACED) == -1) {
        if (errno != EINTR)
            perror("waitpid");
    }

    return status;
}

int builtin_export(int argc, char **argv) {
    int ret;
    size_t equal_pos;

    if (argc > 1) {
        for (equal_pos = 0; argv[1][equal_pos]; equal_pos++) {
            if (argv[1][equal_pos] == '=') break;
        }

        if (argv[1][equal_pos] != '=') {
            return -1;
        }

        argv[1][equal_pos] = '\0';
    }

    if (argc == 2) {
        if ((ret = setenv(argv[1], argv[1] + equal_pos + 1, 1)) == -1) {
            perror("setenv");
        }

        return ret;
    } else if (argc == 3) {
        if ((ret = setenv(argv[1], argv[2], 1)) == -1) {
            perror("setenv");
        }

        return ret;
    }

    return -1;
}

int builtin_bg(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "bg: Usage bg %%[job id]\n");
        return -1;
    }

    if (argv[1][0] == '%') { /* can only bg jobs from the jobs list */
        int rc = run_background(atoi(argv[1] + 1));
        if (rc == -1) {
            fprintf(stderr, "bg: Job not found: %d\n", atoi(argv[1] + 1));
        }

        return rc;
    }

    return -1;
}

int builtin_fg(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "fg: Usage fg %%[job id]\n");
        return -1;
    }

    if (argv[1][0] == '%') { /* can only fg jobs from the jobs list */
        int rc = run_foreground(atoi(argv[1] + 1));
        if (rc == -1) {
            fprintf(stderr, "fg: Job not found: %d\n", atoi(argv[1] + 1));
        }

        return rc;
    }

    return -1;
}

int builtin_kill(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "kill: Usage kill [status] %%[job id] or kill [status] [pid]\n");
        return -1;
    }

    if (argv[2][0] == '%') { /* signal a job in the jobs list */
        return signal_job(atoi(argv[2] + 1), atoi(argv[1]));
    } else { /* signal an arbitrary process */
        return kill(atoi(argv[2]), atoi(argv[1]));
    }
}

int execute_builtin(int argc, char **argv, int *status) {
    switch (argv[0][0]) {
    case 'b': // bg
        if (strcmp(argv[0], "bg") == 0) {
            *status = builtin_bg(argc, argv);
            return 1;
        }
        break;
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
        if (strcmp(argv[0], "export") == 0) {
            *status = builtin_export(argc, argv);
            return 1;
        } if (strcmp(argv[0], "exit") == 0) {
            *status = 0;
            exit(0);
        }
        break;
    case 'f': // fg
        if (strcmp(argv[0], "fg") == 0) {
            *status = builtin_fg(argc, argv);
            return 1;
        }
        break;
    case 'j': // jobs
        if (strcmp(argv[0], "jobs") == 0) {
            // for (int i = 1; i < STACK_SIZE; i++) {
            //     if (job_stack.indices[i] == 0) {
            //         printf("[%d] %d\n", i, job_stack.jobs[i].pid);
            //     }
            // }

            print_jobs();
            *status = 0;
            return 1;
        }
        break;
    case 'k': // kill
        if (strcmp(argv[0], "kill") == 0 && argc == 3) {
            *status = builtin_kill(argc, argv);
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

int execute_forkable_builtin(int argc, char **argv, int *status) {
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

void run_redirects(ASTNode *redirects) {
    int fd;
    while (redirects && redirects->right) {
        switch (redirects->token.token) {
        case T_GREATER:
            if ((fd = open(redirects->right->token.text, O_WRONLY | O_CREAT)) != -1) {
                fchmod(fd, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
                dup2(fd, STDOUT_FILENO);
            } else {
                perror("open");
            }
            break;
        case T_LESS:
            if ((fd = open(redirects->right->token.text, O_RDONLY)) != -1) {
                fchmod(fd, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
                dup2(fd, STDIN_FILENO);
            } else {
                perror("open");
            }
            break;
        case T_GREATER_GREATER:
            if ((fd = open(redirects->right->token.text, O_WRONLY | O_APPEND | O_CREAT)) != -1) {
                fchmod(fd, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
                dup2(fd, STDOUT_FILENO);
            } else {
                perror("open");
            }
            break;
        case T_LESS_GREATER:
            if ((fd = open(redirects->right->token.text, O_RDWR)) != -1) {
                fchmod(fd, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
                dup2(fd, STDIN_FILENO);
                dup2(fd, STDOUT_FILENO);
            } else {
                perror("open");
            }
            break;
        case T_GREATER_AMP:
            if ((fd = open(redirects->right->token.text, O_WRONLY | O_CREAT)) != -1) {
                fchmod(fd, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
                dup2(fd, STDERR_FILENO);
            } else {
                perror("open");
            }
            break;
        case T_GREATER_GREATER_AMP:
            if ((fd = open(redirects->right->token.text, O_WRONLY | O_APPEND | O_CREAT)) != -1) {
                fchmod(fd, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
                dup2(fd, STDERR_FILENO);
            } else {
                perror("open");
            }
            break;
        case T_WORD:
            return;
        default:
            fprintf(stderr, "quash: error processing redirection list\n");
            return;
        }

        redirects = redirects->left;
    }
}

int run_command(ASTNode *ast, int argc, char **argv, job_t job, int pipe_in, int pipe_out, int async) {
    volatile pid_t pid;
    int status;

    if (execute_builtin(argc, argv, &status)) {
        return status;
    }

    set_tstp_longjump_handler();

    if (sigsetjmp(from_suspended, 1)) {
        /* child was suspended */
        printf("%d was suspended\n", pid);
        ignore_tstp();
        return 0;
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

        run_redirects(ast);

        int builtin_status;
        if (execute_forkable_builtin(argc, argv, &builtin_status)) {
            if (builtin_status == -1) {
                perror(argv[0]);
            }

            exit(builtin_status);
        } else if (execvp(argv[0], argv) == -1) {
            perror(argv[0]);
        }

        exit(-1);
    } else {
        if (job == 0) {
            job = create_job();
        }

        register_process(ast, job, pid);

        if (async) {
            printf("[%d] %d\n", job, pid);
        } else {
            status = wait_job(pid);

            if (WIFEXITED(status)) {
                status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                status = WTERMSIG(status);
                printf("%d - %s\n", status, strsignal(status));
            } else {
                status = -1;
            }

            free_job(job);
        }
    }

    ignore_tstp();
    return status;
}

int eval_command(ASTNode *ast, job_t job, int pipe_in, int pipe_out, int async) {
    if (!(ast->token.token == T_WORD || redirect(ast->token))) {
        fprintf(stderr, "quash: syntax error\n");
        return 0;
    }

    int argc = 0;
    char **argv;
    ASTNode *commands = get_commands(ast);

    /* nothing to evaluate */
    if (commands == NULL) {
        fprintf(stderr, "quash: syntax error\n");
        return 0;
    }

    ASTNode *node = commands;
    while (node && node->token.token == T_WORD) {
        argc++;
        node = node->left;
    }

    argv = malloc((argc + 1) * sizeof *argv);

    node = commands;
    for (int i = 0; i < argc; i++) {
        /* should be a linked list of words at this point */
        if (node->right) {
            fprintf(stderr, "quash: syntax error\n");
            free(argv);
            return 0;
        }

        argv[i] = node->token.text;
        node = node->left;
    }

    argv[argc] = NULL;

    int status = run_command(ast, argc, argv, job, pipe_in, pipe_out, async);

    free(argv);
    return status == 0;
}

int eval_pipeline(ASTNode *ast, int *pipe_out, job_t job, int async) {
    int status;
    if (ast->token.token != T_PIPE || !ast->left || !ast->right) {
        fprintf(stderr, "quash: syntax error\n");
        return 0;
    }

    if (ast->left->token.token == T_WORD) {
        /* base case */
        int fds[2];
        pipe(fds);
        eval_command(ast->left, job, -1, fds[1], async);
        close(fds[1]);

        if (pipe_out) {
            int fds2[2];
            pipe(fds2);

            status = eval_command(ast->right, job, fds[0], fds2[1], async);
            close(fds2[1]);
            close(fds[0]);
            *pipe_out = fds2[0];
        } else {
            status = eval_command(ast->right, job, fds[0], -1, async);
            close(fds[0]);
        }

        return status;
    }

    int pipe_in;
    eval_pipeline(ast->left, &pipe_in, job, async);

    if (pipe_out) {
        int fds[2];
        pipe(fds);
        status = eval_command(ast->right, job, pipe_in, fds[1], async);
        close(fds[1]);
        close(pipe_in);
        *pipe_out = fds[0];
    } else {
        status = eval_command(ast->right, job, pipe_in, -1, async);
        close(pipe_in);
    }

    return status;
}

/**
 * Evaluate an abstract syntax tree. Returns `1` if evaluation is successful, else `0`.
 * 
 * @param ast a pointer to an abstract syntax tree returned from the parser
 * @param async a flag whether or not to run the command asynchronously
 * @return `1` on success, `0` on error.
 */
int eval(ASTNode *ast, int async) {
    if (ast == NULL) {
        /* doing nothing is always a success! */
        return 1;
    }

    if (ast->token.token == T_AMP) {
        /* run async and don't set any exit status */
        eval(ast->left, 1);
        
        /* might as well support having commands to the right of an `& */
        return eval(ast->right, async);
    }

    if (ast->token.token == T_AMP_AMP) {
        if (eval(ast->left, async)) {
            return eval(ast->right, async);
        }

        return 0;
    }

    if (ast->token.token == T_PIPE_PIPE) {
        if (!eval(ast->left, async)) {
            return eval(ast->right, async);
        }

        return 1;
    }

    if (ast->token.token == T_PIPE) {
        /* create a job for the pipeline */
        job_t job = create_job();
        return eval_pipeline(ast, NULL, job, async);
    }

    if (ast->token.token == T_WORD || redirect(ast->token)) {
        /* create a job for a single command */
        // job_t job = create_job();
        return eval_command(ast, 0, -1, -1, async);
    }

    return 0;
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
        line = readline(prompt);

        if (sigsetjmp(env, 1)) {
            /* https://lists.gnu.org/archive/html/bug-readline/2016-04/msg00071.html */
            rl_free_line_state();
            rl_cleanup_after_signal();

            RL_UNSETSTATE(RL_STATE_ISEARCH|RL_STATE_NSEARCH|RL_STATE_VIMOTION|RL_STATE_NUMERICARG|RL_STATE_MULTIKEY);
            rl_line_buffer[rl_point = rl_end = rl_mark = 0] = 0;
            rl_callback_handler_remove();
            printf("\n");
            continue;
        }

        TokenDynamicArray tokens;

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

        ASTNode *ast = parse_ast(&tokens);
        // print_parse_tree(ast);
        // printf("%d\n", eval(ast, 0));
        eval(ast, 0);

        free_parse_tree(ast);
        free_token_array(&tokens);
        free(line);
    }

    return 0;
}

int main() {
    init_job_stack();
    init_signal_handlers();

    interactive_prompt();

    cleanup_jobs();
    clear_history();
    return 0;
}
