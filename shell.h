#ifndef __QUASH_EVAL_H__
#define __QUASH_EVAL_H__

#include "parser.h"

void run_command(Command *command, int pipe_in, int pipe_out, int asynchronous, int *return_value);
void run_pipeline(Pipeline *p);

#endif /* __QUASH_EVAL_H__ */