#ifndef __QUASH_JOBS_H__
#define __QUASH_JOBS_H__

#include "quash.h"

void init_job_stack();
job_t create_new_job();
int finish_job(job_t job);
void print_jobs();
int signal_job(job_t job, int signal);
int run_foreground(job_t job);
int run_background(job_t job);

#endif /* __QUASH_JOBS_H__ */