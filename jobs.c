#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include "quash.h"
#include "hash.h"
#include "tokenizer.h"
#include "parser.h"

/*
push_new_job(Job*) -> job_id_t
add_process(job_id_t, Process*) -> none
suspend_job(job_id_t) -> none
resume_job(job_id_t, bool: wait) -> none

in sigchld:
notify_jobs() -> mark all jobs with no active processes as finished

execution:
run_foreground(job_id_t) -> executes job and sets return status in $?
run_background(job_id_t) -> runs async in background, don't set $?
*/

struct {
    Job jobs[JOBS_MAX];
    job_t indices[JOBS_MAX];
    JobHashTable pid_to_job;
} job_stack;

void init_job_stack() {
    /* from now on we just ignore the first entry so job_ids map one-to-one with indices in the stack */
    memset(&job_stack.jobs, 0, JOBS_MAX * sizeof *job_stack.jobs);

    for (size_t n = 0; n < JOBS_MAX; n++) {
        job_stack.indices[n] = n;
        job_stack.jobs[n].id = n;
    }

    init_hash_table(&job_stack.pid_to_job);
}

void cleanup_jobs() {
    free_hash_table_buckets(&job_stack.pid_to_job);
}

/* return the lowest available index */
static job_t next_job_index() {
    /* find first nonzero entry in `indices`, zero it out, and return it */
    for (size_t n = 1; n < JOBS_MAX; n++) {
        if (job_stack.indices[n] != 0) {
            int index = job_stack.indices[n];
            job_stack.indices[n] = 0;
            return index;
        }
    }

    return -1; /* stack is full */
}

static void add_flags(job_t job, int flags) {
    if (job_stack.indices[job] == 0) {
        job_stack.jobs[job].flags |= flags;
    }
}

void print_job(job_t job) {
    Process *process = job_stack.jobs[job].processes;

    printf("[%d]", job);

    for (; process; process = process->next) {
        printf("\t%d\t%s\n", process->pid, process->cmd);
    }
    // printf("\n");
}

static char* ast_to_cmd(ASTNode *ast) {
    if (!(ast->token.token == T_WORD || redirect(ast->token))) {
        return NULL;
    }
    /* TODO not implemented */
    ASTNode *commands = get_commands(ast);
    ASTNode *node = commands;
    size_t len = 0;

    while (node && node->token.token == T_WORD) {
        len += strlen(node->token.text) + 1;
        node = node->left;
    }

    char *cmd = malloc(len + 1);

    node = commands;
    size_t end = 0;
    while (node && node->token.token == T_WORD) {
        strcpy(cmd + end, node->token.text);
        end += strlen(node->token.text);
        strcpy(cmd + end, " ");
        end += 1;
        node = node->left;
    }

    cmd[len] = '\0';

    return cmd;
}

static int append_process(Job *job, ASTNode *ast, pid_t pid) {
    Process *node = job->processes;

    if (node) {
        for (; node != NULL;) {
            if (node->next) {
                node = node->next;
            } else {
                break;
            }
        }

        node->next = malloc(sizeof *node);
        node = node->next;
    } else {
        job->processes = malloc(sizeof *job->processes);
        node = job->processes;
    }

    node->pid = pid;
    node->cmd = ast_to_cmd(ast);
    node->next = NULL;
    job->process_count++;

    hash_table_insert(&job_stack.pid_to_job, pid, job);
    return 1;
}

job_t create_job() {
    return next_job_index();
}

static void free_processes(Process *process) {
    if (!process) {
        return;
    }

    hash_table_delete(&job_stack.pid_to_job, process->pid);

    free_processes(process->next);
    free(process->cmd);
    free(process);
}

void free_job(job_t job) {
    if (job_stack.indices[job] != 0) {
        return;
    }

    free_processes(job_stack.jobs[job].processes);
    job_stack.indices[job] = job;
    job_stack.jobs[job].processes = NULL;
    job_stack.jobs[job].process_count = 0;
    job_stack.jobs[job].flags = 0;
}

int register_process(ASTNode *ast, job_t job, pid_t pid) {
    if (job_stack.indices[job] != 0) {
        return 0;
    }

    return append_process(&job_stack.jobs[job], ast, pid);
}

void print_jobs() {
    for (int job = 1; job < JOBS_MAX; job++) {
        if (job_stack.indices[job] == 0) {
            print_job(job);
        }
    }
}

int signal_job(job_t job, int signal) {
    if (job_stack.indices[job] != 0) {
        return -1;
    }

    Process *process = job_stack.jobs[job].processes;

    for (; process; process = process->next) {
        if (kill(process->pid, signal) == -1) {
            perror("kill");
            return -1;
        }
    }

    return 0;
}

int run_foreground(job_t job) {
    if (job_stack.indices[job] != 0) {
        return -1;
    }

    volatile int return_value = 0;
    Process *process = job_stack.jobs[job].processes;

    for (; process; process = process->next) {
        int status;

        if (kill(process->pid, SIGCONT) == -1) {
            perror("kill");
            return -1;
        }

        if (waitpid(process->pid, &status, 0) == -1) {
            perror("waitpid");
            return -1;
        }

        if (WIFEXITED(status)) {
            return_value = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            return_value = WTERMSIG(status);
            printf("%d - %s\n", return_value, strsignal(return_value));
        } else {
            return_value = -1;
        }
    }

    return return_value;
}

int run_background(job_t job) {
    if (job_stack.indices[job] != 0) {
        return -1;
    }

    // printf("[%d] %d\n", job, pid);
    add_flags(job, JOB_ASYNC);
    Process *process = job_stack.jobs[job].processes;

    for (; process; process = process->next) {
        if (kill(process->pid, SIGCONT) == -1) {
            perror("kill");
            return -1;
        }
    }

    return 0;
}

Job* get_job_from_pid(pid_t pid) {
    return hash_table_get(&job_stack.pid_to_job, pid);
}

int all_completed(job_t job) {
    if (job_stack.indices[job] != 0) {
        return 0;
    }

    Process *process = job_stack.jobs[job].processes;
    for (; process; process = process->next) {
        if ((process->flags & JOB_FINISHED) == 0) {
            return 0;
        }
    }
    /* TODO erm */
    return 1;
}
