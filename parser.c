#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "tokenizer.h"
#include "string_buf.h"
#include "parser.h"

// typedef struct _ParserState {
//     TokenDynamicArray *tokens;
// } ParserState;

struct {
    TokenDynamicArray *tokens;
    size_t token_index;
} parser_state;

static Redirect* eval_redirection();
static Redirect* eval_redirection_list();
static Command* eval_command();
static Pipeline* eval_pipeline();
// static void eval_conditional();

static TokenTuple peek(size_t offset);
static TokenTuple current_tuple();
static Token current_token();
static void advance();
static int consume(Token t);

Pipeline* eval(TokenDynamicArray *tokens) {
    parser_state.tokens = tokens;
    parser_state.token_index = 0;

    return eval_pipeline();
}
/*
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
*/
static Pipeline* eval_pipeline() {
    Pipeline *pipeline = malloc(sizeof *pipeline);
    pipeline->count = 0;
    pipeline->asynchronous = 0;

    pipeline->commands = eval_command();
    pipeline->count++;

    Command *head = pipeline->commands;
    for (;;) {
        if (consume(T_PIPE)) {
            head->next = eval_command();
            head = head->next;
            pipeline->count++;
        } else if (consume(T_PIPE_AMP)) {
            // need to redirect stderr of $1 to stdin of $2
            head->next = eval_command();
            head = head->next;
            pipeline->count++;
        } else {
            head->next = NULL;
            break;
        }
    }

    if (consume(T_AMP)) {
        pipeline->asynchronous = 1;
    }

    return pipeline;
}

static Command* eval_command() {
    Command *command = malloc(sizeof *command);
    create_string_array(&(command->strings));
    command->argc = 0;
    TokenTuple t = current_tuple();

    while (consume(T_WORD) || consume(T_NUMBER)) {
        // args
        append_string(&(command->strings), t.text, -1);
        command->argc++;
        t = current_tuple();
    }

    if (command->argc == 0) {
        free_string_array(&(command->strings));
        free(command);
        return NULL;
    }

    command->argv = malloc((command->argc + 1) * sizeof *command->argv);
    for (int i = 0; i < command->argc; i++) {
        command->argv[i] = (char*) &(command->strings.buffer[command->strings.strings[i]]);
    }
    command->argv[command->argc] = NULL;

    command->redirects = eval_redirection_list();

    return command;
}

static Redirect* eval_redirection_list() {
    Redirect *redirect = NULL;

    for (;;) {
        switch (current_token()) {
        case T_LESS:
        case T_GREATER:
        case T_GREATER_GREATER:
        case T_LESS_GREATER:
            if (redirect == NULL) {
                redirect = eval_redirection();
            } else {
                redirect->next = eval_redirection();
            }
        default:
            goto redir_list_end;
        }
    }

redir_list_end:
    return redirect;
}

static Redirect* eval_redirection() {
    TokenTuple next = peek(1);
    if (next.token != T_WORD) {
        return NULL;
    }

    Redirect *redirect = malloc(sizeof *redirect);
    redirect->next = NULL;

    switch (current_token()) {
    case T_LESS:
        consume(T_LESS);
        redirect->fp = next.text;
        redirect->instr = RI_READ_FILE;
        advance();
        break;
    case T_GREATER:
        consume(T_GREATER);
        redirect->fp = next.text;
        redirect->instr = RI_WRITE_FILE;
        advance();
        break;
    case T_GREATER_GREATER:
        consume(T_GREATER_GREATER);
        redirect->fp = next.text;
        redirect->instr = RI_WRITE_APPEND_FILE;
        advance();
        break;
    case T_LESS_GREATER:
        consume(T_LESS_GREATER);
        redirect->fp = next.text;
        redirect->instr = RI_READ_WRITE_FILE;
        advance();
        break;
    case T_NUMBER: // 2>&1 TODO not supported yet
        break;
    case T_GREATER_AMP:
        break;
    default:
        break;
        // error probably!
    }

    return redirect;
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

static void advance() {
    parser_state.token_index++;
}

static int consume(Token t) {
    if (parser_state.token_index < parser_state.tokens->length
        && parser_state.tokens->tuples[parser_state.token_index].token == t) {
        parser_state.token_index++;
        return 1;
    }

    return 0;
}
