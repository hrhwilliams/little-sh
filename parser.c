#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <glob.h>

#include "quash.h"
#include "tokenizer.h"
#include "arrays.h"

struct {
    TokenDynamicArray *tokens;
    size_t token_index;
} ParserState;

static void advance() {
    ParserState.token_index++;
}

static Token peek(size_t offset) {
    if (ParserState.token_index + offset < ParserState.tokens->length) {
        return ParserState.tokens->tuples[ParserState.token_index + offset];
    }

    return (Token) { .text = NULL, .token = T_NONE, .flags = 0 };
}

static int consume(TokenEnum t) {
    if (ParserState.token_index < ParserState.tokens->length
        && ParserState.tokens->tuples[ParserState.token_index].token == t) {
        advance();
        return 1;
    }

    return 0;
}

static Token current_token() {
    return peek(0);
}

static int number(Token token) {
    return (token.flags & TF_NUMBER) != 0;
}


typedef struct _BindingPower {
    short left;
    short right;
} BindingPower;

BindingPower get_binding_power(Token t) {
    static BindingPower binding_power[] = {
        [T_WORD]                = { 7, 7 }, /* should have higher precedence */
        [T_GREATER]             = { 5, 6 }, /* should have higher precedence */
        [T_LESS]                = { 5, 6 },
        [T_GREATER_GREATER]     = { 5, 6 },
        [T_LESS_GREATER]        = { 5, 6 },
        [T_GREATER_AMP]         = { 5, 6 },
        [T_GREATER_GREATER_AMP] = { 5, 6 },
        [T_PIPE]                = { 3, 4 }, /* should have lower precedence */
        [T_AMP]                 = { 0, 0 },
        [T_AMP_AMP]             = { 1, 2 },
        [T_PIPE_PIPE]           = { 1, 2 },
    };

    if (t.token >= T_WORD && t.token <= T_PIPE_PIPE) {
        return binding_power[t.token];
    }

    return (BindingPower) { 0, 0 };
}

static ASTNode* ast_node(Token token, ASTNode *left, ASTNode *right) {
    ASTNode *node = malloc(sizeof *node);
    node->left = left;
    node->right = right;
    node->token = token;
    return node;
}

static ASTNode* expression(int min_bp) {
    ASTNode *lhs = NULL;

    for (;;) {
        Token token = current_token();

        if (token.token == T_EOS) {
            break;
        }

        if (consume(T_WORD)) {
            lhs = ast_node(token, expression(7), NULL);
            continue;
        }

        BindingPower bp = get_binding_power(token);

        if (bp.left < min_bp) {
            break;
        }

        advance();
        ASTNode *rhs = expression(bp.right);
        lhs = ast_node(token, lhs, rhs);
    }

    return lhs;
}

ASTNode* parse_ast(TokenDynamicArray *tokens) {
    ParserState.tokens = tokens;
    ParserState.token_index = 0;
    return expression(0);
}

static void _print_parse_tree(ASTNode *tree, int depth) {
    if (!tree) {
        // printf("()");
        return;
    }

    for (int i = 0; i < depth; i++) {
        printf(" - ");
    }

    printf("[%d : %s]\n", tree->token.token, tree->token.text);
    _print_parse_tree(tree->left,   depth + 1);
    _print_parse_tree(tree->right,  depth + 1);
}

void print_parse_tree(ASTNode *tree) {
    _print_parse_tree(tree, 0);
}

void free_parse_tree(ASTNode *tree) {
    if (tree->left) {
        free_parse_tree(tree->left);
    }

    if (tree->right) {
        free_parse_tree(tree->right);
    }

    free(tree);
}

ASTNode* get_commands(ASTNode *ast) {
    if (!(ast->token.token == T_WORD || redirect(ast->token))) {
        return 0;
    }

    int argc = 0;
    char **argv;
    ASTNode *commands = ast;
    ASTNode *redirects = NULL;

    /* walk the tree until `commands` is pointing at words */
    if (redirect(ast->token)) {
        redirects = ast;
        commands = redirects;
        while (commands && redirect(commands->token)) {
            /* check if a redirect has no argument while we walk the AST */
            if (!commands->right) {
                fprintf(stderr, "quash: syntax error\n");
                return NULL;
            }

            commands = commands->left;
        }
    }

    return commands;
}
