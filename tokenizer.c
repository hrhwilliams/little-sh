#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glob.h>

#include "string_buf.h"
#include "tokenizer.h"

typedef struct _TokenizerState {
    TokenDynamicArray tokens;
    StringDynamicBuffer string_buf;
} TokenizerState;

static const char *token_to_string[] = {
    "T_NONE",
    "T_EOF",
    "T_ERROR",
    "T_WORD",
    "T_STRING",
    "T_VARIABLE",
    "T_NUMBER",
    "T_GREATER",
    "T_LESS",
    "T_GREATER_GREATER",
    "T_LESS_LESS",
    "T_LESS_GREATER",
    "T_AMP_GREATER",
    "T_AMP_GREATER_GREATER",
    "T_GREATER_AMP",
    "T_GREATER_GREATER_AMP",
    "T_PIPE",
    "T_PIPE_AMP",
    "T_AMP",
    "T_AMP_AMP",
    "T_PIPE_PIPE",
    "T_PERCENT",
};

void print_token(TokenTuple t) {
    if (t.text) {
        printf("(%s:'%s')", token_to_string[t.token], t.text);
    } else {
        printf("(%s)", token_to_string[t.token]);
    }
}

static void grow_token_array(TokenDynamicArray *array) {
    array->slots *= 2;
    array->tuples = realloc(array->tuples, array->slots * sizeof *array->tuples);
}

static void create_token_array(TokenDynamicArray *array) {
    array->slots = DYNARRAY_DEFAULT_SIZE;
    array->length = 0;
    array->tuples = malloc(DYNARRAY_DEFAULT_SIZE * sizeof *array->tuples);
}

static void append_token(TokenDynamicArray *array, TokenTuple tuple) {
    if (array->length + 1 == array->slots) {
        grow_token_array(array);
    }

    array->tuples[array->length++] = tuple;
}

void free_token_array(TokenDynamicArray *array) {
    for (size_t i = 0; i <array->length; i++) {
        free(array->tuples[i].text);
    }
    free(array->tuples);
}

static int is_whitespace(char c) {
    return c == ' ' || c == '\t';
}

static int is_quote_char(char c) {
    return c == '\'' || c == '\"' || c == '`';
}

static int is_number(char c) {
    return c >= '0' && c <= '9';
}

static int is_printable(char c) {
    return c >= ' ' && c <= '~';
}

static int is_var_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || is_number(c) || c == '_';
}

static int is_metachar(char c) {
    switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '|':
    case '%':
    case '&':
    // case ';':
    case '<':
    case '>':
    // case '(':
    // case ')':
        return 1;
    default:
        return 0;
    }
}

static int is_word_char(char c) {
    return is_printable(c) && !(is_metachar(c) || is_quote_char(c)) && c != '\\';
}

static void tokenize_metachar(char *input, TokenDynamicArray *tokens) {
    int i = 0;
    int len = strlen(input);

    while (i < len) {
        switch (input[i]) {
        case '%':
            append_token(tokens, (TokenTuple) { NULL, T_PERCENT }); i++;
            break;
        case '|':
            if (i+1 < len && input[i+1] == '|') {
                append_token(tokens, (TokenTuple) { NULL, T_PIPE_PIPE }); i += 2;
            } else if (i+1 < len && input[i+1] == '&') {
                append_token(tokens, (TokenTuple) { NULL, T_PIPE_AMP }); i += 2;
            } else {
                append_token(tokens, (TokenTuple) { NULL, T_PIPE }); i++;
            }
            break;
        case '&':
            if (i+1 < len && input[i+1] == '&') {
                append_token(tokens, (TokenTuple) { NULL, T_AMP_AMP }); i += 2;
            } else if (i+1 < len && input[i+1] == '>') {
                if (i+2 < len && input[i+2] == '>') {
                    append_token(tokens, (TokenTuple) { NULL, T_AMP_GREATER_GREATER }); i += 3;
                } else {
                    append_token(tokens, (TokenTuple) { NULL, T_AMP_GREATER }); i += 2;
                }
            } else {
                append_token(tokens, (TokenTuple) { NULL, T_AMP }); i++;
            }
            break;
        case '<':
            if (i+1 < len && input[i+1] == '<') {
                append_token(tokens, (TokenTuple) { NULL, T_LESS_LESS }); i += 2;
            } else if (i+1 < len && input[i+1] == '>') {
                append_token(tokens, (TokenTuple) { NULL, T_LESS_GREATER }); i += 2;
            } else {
                append_token(tokens, (TokenTuple) { NULL, T_LESS }); i++;
            }
            break;
        case '>':
            if (i+1 < len && input[i+1] == '>') {
                if (i+2 < len && input[i+2] == '&') {
                    append_token(tokens, (TokenTuple) { NULL, T_GREATER_GREATER_AMP }); i += 3;
                } else {
                    append_token(tokens, (TokenTuple) { NULL, T_GREATER_GREATER }); i += 2;
                }
            } else if (i+1 < len && input[i+1] == '&') {
                append_token(tokens, (TokenTuple) { NULL, T_GREATER_AMP }); i += 2;
            } else {
                append_token(tokens, (TokenTuple) { NULL, T_GREATER }); i++;
            } 
            break;
        default:
            append_token(tokens, (TokenTuple) { strdup(input), T_ERROR });
            return;
        }
    }
}

void append_variable(char *string, TokenDynamicArray *tokens) {
    char *var = getenv(string);

    if (var) {
        TokenTuple t;
        t.token = T_WORD;
        t.text = strdup(var);
        append_token(tokens, t);
    }
}

static void tokenize_chunk(char *string, TokenDynamicArray *tokens) {
    TokenTuple t;

    switch (string[0]) {
    case '\'':
    case '\"':
    case '`':
        t.token = T_STRING;
        t.text = strdup(string + 1);
        append_token(tokens, t);
        return;
    case '$':
        append_variable(string + 1, tokens);
        return;
    case '>':
    case '<':
    case '&':
    case '|':
    case '%':
        /* need to keep doing this until string is exhausted*/
        tokenize_metachar(string, tokens);
        return;
    }

    int all_numbers = 1;
    for (int i = 0; string[i] != '\0'; i++) {
        if (!is_number(string[i])) {
            all_numbers = 0;
        }
    }

    if (all_numbers) {
        t.token = T_NUMBER;
    } else {
        t.token = T_WORD;
    }

    t.text = strdup(string);
    append_token(tokens, t);
}

static StringDynamicBuffer expand_globs(char *input) {
    static int glob_flags = GLOB_NOSORT | GLOB_NOCHECK;
    glob_t globbuf;
    memset(&globbuf, 0, sizeof globbuf);

    StringDynamicBuffer strings;
    create_string_array(&strings);

    for (int i = 0; input[i] != '\0';) {
        if (is_whitespace(input[i]) || input[i] == '\\') {
            i++;
            continue;
        }

        if (is_word_char(input[i])) {
            int word_start = i;

            /* increment `i` until it is no longer indexing a word char */
            for (i += 1; input[i] && is_word_char(input[i]); i++) { }

            char temp = input[i];
            input[i] = '\0';
            glob(input + word_start, glob_flags, NULL, &globbuf);

            for (size_t j = 0; j < globbuf.gl_pathc; j++) {
                append_string(&strings, globbuf.gl_pathv[j], -1);
            }

            input[i] = temp;
            globfree(&globbuf);
            continue;
        }

        if (input[i] == '$') {
            int var_start = i;
            for (i += 1; input[i] && is_var_char(input[i]); i++) {}

            append_string(&strings, input + var_start, i - var_start);
            continue;
        }

        if (is_metachar(input[i])) {
            int meta_start = i;
            for (i += 1; input[i] && !is_whitespace(input[i]) && is_metachar(input[i]); i++) {}

            append_string(&strings, input + meta_start, i - meta_start);
            continue;
        }

        if (is_quote_char(input[i])) {
            int string_start = i;
            char endquote = input[i];

            for (i += 1; input[i] && input[i] != endquote; i++) { }

            if (input[i] == '\0') {
                // need to communicate this because the string was never closed
            } else {

            }
            append_string(&strings, input + string_start, i - string_start);
            i++;
            continue;
        }
    }

    return strings;
}

TokenDynamicArray tokenize(char *input) {
    StringDynamicBuffer strings = expand_globs(input);
    TokenDynamicArray tokens;
    create_token_array(&tokens);

    for (size_t i = 0; i < strings.strings_used; i++) {
        tokenize_chunk(strings.buffer + strings.strings[i], &tokens);
    }

    free_string_array(&strings);
    return tokens;
}
