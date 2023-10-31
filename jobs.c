#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include "quash.h"

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
} job_stack;

void init_job_stack() {
    /* from now on we just ignore the first entry so job_ids map one-to-one with indices in the stack */
    for (size_t n = 0; n < JOBS_MAX; n++) {
        job_stack.indices[n] = n;
    }

    memset(&job_stack.jobs, 0, JOBS_MAX * sizeof *job_stack.jobs);
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

static void print_job(job_t job) {
    Process *process = job_stack.jobs[job].processes;

    printf("[%d]", job);

    for (; process; process = process->next) {
        printf("\t%d\n", process->pid);
    }
}

job_t create_new_job() {
    return next_job_index();
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

    int return_value;
    Process *process = job_stack.jobs[job].processes;

    for (; process; process = process->next) {
        int status;

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

job_t create_job(ASTNode *ast) {
    /* TODO set up a job in the job list */
    /* TODO copy input string from the AST */
    return 0;
}

/* TODO might need a mutex! */
void add_command(ASTNode *ast, job_t job, pid_t pid) {

}
