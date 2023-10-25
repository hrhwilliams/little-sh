#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "tokenizer.h"
#include "parser.h"

typedef struct _ParserState {

} ParserState;

static void eval_redirection_list();
static void eval_command(int pipe_in, int pipe_out);
static void eval_pipeline();
static void eval_conditional();

static int redirect(int fd);
static int last_successful();

static int consume(Token t);
static int peek_is(Token t, int lookahead);

int eval(TokenDynamicArray *tokens) {

}

static void eval_conditional() {
    eval_pipeline();

    for (;;) {
        if (consume(T_AMP_AMP) && last_successful()) {
            eval_pipeline();
        } else if (consume(T_PIPE_PIPE) && !last_successful()) {
            eval_pipeline();
        } else {
            break;
        }
    }
}

static void eval_pipeline() {
    int fds[2];
    pipe(fds);

    eval_command(-1, -1);

    for (;;) {
        if (consume(T_PIPE)) {
            
        } else if (consume(T_PIPE_AMP)) {
            
        } else {
            break;
        }
    }
}

static void eval_command(int pipe_in, int pipe_out) {
    
}

static int consume(Token t) {
    return 0;
}
