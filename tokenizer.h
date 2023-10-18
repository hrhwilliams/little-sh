#ifndef __QUASH_TOKENIZER_H__
#define __QUASH_TOKENIZER_H__

#include <stddef.h>

#define DYNARRAY_DEFAULT_SIZE 8

typedef enum Token {
    T_NONE,
    T_EOF,
    T_ERROR,
    T_WORD,
    T_STRING,
    T_VARIABLE,
    T_NUMBER,
    T_GREATER,
    T_LESS,
    T_GREATER_GREATER,
    T_LESS_LESS,
    T_LESS_GREATER,
    T_AMP_GREATER,
    T_AMP_GREATER_GREATER,
    T_GREATER_AMP,
    T_GREATER_GREATER_AMP,
    T_PIPE,
    T_PIPE_AMP,
    T_AMP,
    T_AMP_AMP,
    T_PIPE_PIPE
} Token;

typedef struct TokenTuple {
    char *text;
    Token token;
} TokenTuple;

typedef struct TokenDynamicArray {
    TokenTuple *tuples;
    size_t length;
    size_t slots;
} TokenDynamicArray;

TokenDynamicArray tokenize(char *input);
void print_token(TokenTuple t);
void free_token_array(TokenDynamicArray *array);

#endif /* __QUASH_TOKENIZER_H__  */