#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <glob.h>

#include "quash.h"
#include "arrays.h"

struct {
    TokenDynamicArray *tokens;
    size_t token_index;
} parser_state;

static void advance() {
    parser_state.token_index++;
}

static Token peek(size_t offset) {
    if (parser_state.token_index + offset < parser_state.tokens->length) {
        return parser_state.tokens->tuples[parser_state.token_index + offset];
    }

    return (Token) { .text = NULL, .token = T_NONE, .flags = 0 };
}

static int consume(TokenEnum t) {
    if (parser_state.token_index < parser_state.tokens->length
        && parser_state.tokens->tuples[parser_state.token_index].token == t) {
        advance();
        return 1;
    }

    return 0;
}

static Token current_token() {
    return peek(0);
}

static int operator(Token token) {
    return (token.flags & TF_OPERATOR) != 0; 
}


typedef struct _BindingPower {
    short left;
    short right;
} BindingPower;

BindingPower get_binding_power(Token t) {
    static BindingPower binding_power[] = {
        [T_WORD]                = { 0, 0 }, /* should have higher precedence */
        [T_GREATER]             = { 1, 2 }, /* should have higher precedence */
        [T_LESS]                = { 1, 2 },
        [T_GREATER_GREATER]     = { 1, 2 },
        [T_LESS_GREATER]        = { 1, 2 },
        [T_GREATER_AMP]         = { 2, 3 },
        [T_GREATER_GREATER_AMP] = { 1, 2 },
        [T_PIPE]                = { 1, 1 }, /* should have lower precedence */
        [T_AMP]                 = { 1, 0 },
        [T_AMP_AMP]             = { 0, 0 },
        [T_PIPE_PIPE]           = { 0, 0 },
    };

    if (t.token >= T_GREATER && t.token <= T_AMP) {
        if (t.token == T_WORD && (t.flags & TF_NUMBER)) {
            return (BindingPower) { 1, 1 };
        }
        return binding_power[t.token];
    }

    return (BindingPower) { 0, 0 };
}

/*
            conditional
             /        \
         pipeline    pipeline
        /     |          |
   command command   command
                        |
                     redirects
 */

typedef struct _ASTNode {
    struct _ASTNode *left;
    struct _ASTNode *right;
    Token token;
} ASTNode;

ASTNode* ast_node(Token token, ASTNode *left, ASTNode *right) {
    ASTNode *node = malloc(sizeof *node);
    node->left = left;
    node->right = right;
    node->token = token;
    return node;
}

ASTNode* expression(int min_bp) {
    ASTNode *lhs;
    Token current = current_token();

    if (!consume(T_WORD)) {
        return NULL;
    }

    for (;;) {
        current = current_token();
        if (current.token == T_WORD) {
            /* need to check binding power of next */
            lhs->left = ast_node(current, NULL, NULL);
            lhs->right = NULL;
            lhs = lhs->left;
        } else if (operator(current)) {

        }

        BindingPower bp = get_binding_power(current);
        if (bp.left < min_bp) {
            break;
        }

        advance();
        ASTNode *rhs = expression(bp.right);
        lhs = ast_node(current, lhs, rhs);
    }

    return lhs;
}

// echo "hello world" > out.txt -> (echo hello world) (> out.txt)
//
// echo hello world 2>&1 > out.txt | something else -> ((echo hello world) (2>&1 > out.txt)) | (something else)
//
// by convention: when lhs and rhs are interchangable:
// left-hand side holds linked list of tokens, right-hand side are arguments


Redirect* eval_redirection() {
    Token next = peek(1);

    Redirect *redirect = malloc(sizeof *redirect);
    redirect->next = NULL;

    switch (current_token().token) {
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
    case T_WORD: // 2>&1
        redirect->fds[0] = atoi(current_token().text);
        redirect->fds[1] = atoi(peek(2).text);
        redirect->fp = NULL;
        redirect->instr = RI_REDIRECT_FD;
        break;
    default:
        break;
        // error probably!
    }

    return redirect;
}

Redirect* eval_redirection_list() {
    Redirect *redirect = NULL;

    for (;;) {
        switch (current_token().token) {
        case T_WORD:
            if ((current_token().flags & TF_NUMBER) && (peek(1).token == T_GREATER_AMP) && (peek(2).flags & TF_NUMBER)) {

            } else {
                break;
            }
        case T_LESS:
        case T_GREATER:
        case T_GREATER_GREATER:
        case T_LESS_GREATER:
            if (redirect == NULL) {
                redirect = eval_redirection();
            } else {
                redirect->next = eval_redirection();
                redirect = redirect->next;
            }
            redirect->next = NULL;
            break;
        default:
            goto redir_list_end;
        }
    }

redir_list_end:
    return redirect;
}

Command* eval_command() {
    Command *command = malloc(sizeof *command);
    create_string_array(&(command->strings));
    command->argc = 0;
    Token t = current_token();

    while (consume(T_WORD)) {
        // args
        append_string(&(command->strings), t.text, -1);
        command->argc++;
        t = current_token();
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

Pipeline* eval_pipeline() {
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

/**
 * @param pipeline a pipeline to free the memory of
 */
void free_pipeline(Pipeline *pipeline) {
    Command *command = pipeline->commands;

    while (command) {
        free_string_array(&command->strings);
        free(command->argv);
        Command *next = command->next;
        free(command);
        command = next;
    }

    free(pipeline);
}

/**
 * Takes in an array of tokens and attempts to parse the tokens into a pipeline
 * that can then be evaluated, or returns `NULL` on a parser error.
 * 
 * @param tokens an array of tokens to parse
 * @return a pipeline ready to be evaluated
 */
Pipeline* parse(TokenDynamicArray *tokens) {
    parser_state.tokens = tokens;
    parser_state.token_index = 0;

    Pipeline *p = eval_pipeline();
    if (!consume(T_EOS)) {
        free_pipeline(p);
        return NULL;
    }

    return p;
}
