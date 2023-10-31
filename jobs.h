#ifndef __QUASH_JOBS_H__
#define __QUASH_JOBS_H__

#include "quash.h"

void init_job_stack();
void cleanup_jobs();
job_t create_job();
int register_process(ASTNode *ast, job_t job, pid_t pid);
Job* get_job_from_pid(pid_t pid);
void free_job(job_t job);
void print_jobs();
int signal_job(job_t job, int signal);
int run_foreground(job_t job);
int run_background(job_t job);

#endif /* __QUASH_JOBS_H__ */
