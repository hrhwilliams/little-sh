#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "tokenizer.h"
#include "parser.h"

// typedef struct _ParserState {
//     TokenDynamicArray *tokens;
// } ParserState;

struct {
    TokenDynamicArray *tokens;
    size_t token_index;
} parser_state;

static void eval_redirection_list();
static Command* eval_command();
static void eval_pipeline();
static void eval_conditional();

static int redirect(int fd);
static int last_successful();

static int consume(Token t);
static int peek_is(Token t, int lookahead);

int eval(TokenDynamicArray *tokens) {
    parser_state.tokens = tokens;
    parser_state.token_index = 0;
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
    Pipeline *pipeline = malloc(sizeof *pipeline);
    eval_command();

    for (;;) {
        if (consume(T_PIPE)) {
            eval_command();
        } else if (consume(T_PIPE_AMP)) {
            // need to redirect stderr of $1 to stdin of $2
            eval_command();
        } else {
            break;
        }
    }
}

static Command* eval_command() {
    Command *command = malloc(sizeof *command);
    TokenTuple t = current_tuple();

    if (consume(T_WORD) || consume(T_NUMBER)) {
        // command name
    }

    while (consume(T_WORD) || consume(T_NUMBER)) {
        // args
    }

    eval_redirection_list();

    return command;
}

static void eval_redirection_list() {
    Redirect *redirect = malloc(sizeof *redirect);

    for (;;) {
        switch (current_token()) {
        case T_LESS:
            break;
        case T_GREATER:
            break;
        case T_GREATER_GREATER:
            break;
        case T_LESS_GREATER:
            break;
        case T_NUMBER: // 2>&1
            break;
        case T_GREATER_AMP:
            break;
        }
    }
}

static TokenTuple peek(size_t offset) {
    if (parser_state.token_index + offset < parser_state.tokens->length) {
        return parser_state.tokens->tuples[parser_state.token_index + offset];
    }

    return (TokenTuple) { .token = T_NONE, .text = NULL };
}

static TokenTuple current_tuple() {
    return peek(0);
}

static Token current_token() {
    return peek(0).token;
}

static int consume(Token t) {
    if (parser_state.token_index < parser_state.tokens->length
        && parser_state.tokens->tuples[parser_state.token_index].token == t) {
        parser_state.token_index++;
        return 1;
    }

    return 0;
}
