#include <stdio.h>
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
        [T_WORD]                = { 5, 5 }, /* should have higher precedence */
        [T_GREATER]             = { 3, 4 }, /* should have higher precedence */
        [T_LESS]                = { 3, 4 },
        [T_GREATER_GREATER]     = { 3, 4 },
        [T_LESS_GREATER]        = { 0, 0 },
        [T_GREATER_AMP]         = { 0, 0 },
        [T_GREATER_GREATER_AMP] = { 0, 0 },
        [T_PIPE]                = { 2, 3 }, /* should have lower precedence */
        [T_AMP]                 = { 0, 0 },
        [T_AMP_AMP]             = { 1, 2 },
        [T_PIPE_PIPE]           = { 1, 2 },
    };

    if (t.token >= T_GREATER && t.token <= T_AMP) {
        if (t.token == T_WORD && (t.flags & TF_NUMBER)) {
            return (BindingPower) { 1, 1 };
        }
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
        Token current = current_token();

        if (current.token == T_EOS) {
            break;
        }

        if (consume(T_WORD)) {
            /* treat words as having binding power of 5 to each other */
            lhs = ast_node(current, expression(5), NULL);
            continue;
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

ASTNode* parse_ast(TokenDynamicArray *tokens) {
    parser_state.tokens = tokens;
    parser_state.token_index = 0;
    return expression(0);
}

void print_parse_tree(ASTNode *tree) {
    if (!tree) {
        // printf("()");
        return;
    }

    printf("([%d : %s] ", tree->token.token, tree->token.text);
    print_parse_tree(tree->left);
    print_parse_tree(tree->right);
    printf(")");
}
