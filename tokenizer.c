#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glob.h>

#include "tokenizer.h"

typedef struct StringDynamicArray{
    int *offsets;
    char *buf;
    size_t length;
    size_t slots;
    size_t buf_length;
    size_t buf_slots;
} StringDynamicArray;

static void grow_token_array(TokenDynamicArray *array) {
    array->slots *= 2;
    array->tuples = realloc(array->tuples, array->slots * sizeof *array->tuples);
}

static void grow_string_array(StringDynamicArray *array) {
    array->slots *= 2;
    array->offsets = realloc(array->offsets, array->slots * sizeof *array->offsets);
}

static void grow_string_buf(StringDynamicArray *array) {
    array->buf_slots *= 2;
    array->buf = realloc(array->buf, array->buf_slots * sizeof *array->buf);
}

static void create_token_array(TokenDynamicArray *array) {
    array->slots = DYNARRAY_DEFAULT_SIZE;
    array->length = 0;
    array->tuples = malloc(DYNARRAY_DEFAULT_SIZE * sizeof *array->tuples);
}

static void create_string_array(StringDynamicArray *array) {
    array->slots = DYNARRAY_DEFAULT_SIZE;
    array->length = 0;
    array->offsets = malloc(DYNARRAY_DEFAULT_SIZE * sizeof *array->offsets);

    array->buf_slots = STRING_DYNARRAY_BUF_SIZE;
    array->buf_length = 0;
    array->buf = malloc(STRING_DYNARRAY_BUF_SIZE * sizeof *array->buf);
}

static void append_token(TokenDynamicArray *array, TokenTuple tuple) {
    if (array->length + 1 == array->slots) {
        grow_token_array(array);
    }

    array->tuples[array->length++] = tuple;
}

static void append_string(StringDynamicArray *array, char *string, int bytes) {
    size_t len = strlen(string);

    if (array->length + 1 == array->slots) {
        grow_string_array(array);
    }

    while (array->buf_length + len + 1 >= array->buf_slots) {
        grow_string_buf(array);
    }

    array->offsets[array->length++] = array->buf_length;

    if (bytes == -1) {
        strncpy(array->buf + array->buf_length, string, len + 1);
    } else {
        strncpy(array->buf + array->buf_length, string, bytes + 1);
        (array->buf + array->buf_length)[bytes] = '\0';
    }

    array->buf_length += len + 1;
}

void free_token_array(TokenDynamicArray *array) {
    for (int i = 0; i <array->length; i++) {
        free(array->tuples[i].text);
    }
    free(array->tuples);
}

static void free_string_array(StringDynamicArray *array) {
    free(array->offsets);
    free(array->buf);
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

static int is_escape_char(char c) {
    return c == '\\';
}

static int is_printable(char c) {
    return c >= ' ' && c <= '~';
}

static int is_delimiter(char c) {
    return c == ' ' || c == '\n' || c == '\t';
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
    return is_printable(c) && !(is_metachar(c) || is_quote_char(c));
}

static void tokenize_metachar(char *input, TokenDynamicArray *tokens) {
    int i = 0;
    int len = strlen(input);

    while (i < len) {
        switch (input[i]) {
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
        t.token = T_VARIABLE;
        t.text = strdup(string + 1);
        append_token(tokens, t);
        return;
    case '>':
    case '<':
    case '&':
    case '|':
        /* need to keep doing this until string is exhausted*/
        tokenize_metachar(string, tokens);
        return;
    }

    t.token = T_WORD;
    t.text = strdup(string);
    append_token(tokens, t);
}

static StringDynamicArray expand_globs(char *input) {
    glob_t globbuf;
    memset(&globbuf, 0, sizeof globbuf);

    StringDynamicArray strings;
    create_string_array(&strings);

    int glob_flags = GLOB_NOSORT | GLOB_NOCHECK | GLOB_NOMAGIC;

    for (int i = 0; input[i] != '\0';) {
        if (is_whitespace(input[i])) {
            i++;
            continue;
        }

        if (is_word_char(input[i])) {
            int word_start = i;
            for (i += 1; input[i] && is_word_char(input[i]); i++) { }

            char temp = input[i];
            input[i] = '\0';
            glob(input + word_start, glob_flags, NULL, &globbuf);

            for (int j = 0; j < globbuf.gl_pathc; j++) {
                append_string(&strings, globbuf.gl_pathv[j], -1);
            }

            input[i] = temp;
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
                // need to communicate this
            } else {

            }
            append_string(&strings, input + string_start, i - string_start);
            i++;
            continue;
        }
    }

    if (globbuf.gl_pathc > 0) {
        globfree(&globbuf);
    }

    return strings;
}

TokenDynamicArray tokenize(char *input) {
    StringDynamicArray strings = expand_globs(input);

    TokenDynamicArray tokens;
    create_token_array(&tokens);

    for (int i = 0; i < strings.length; i++) {
        tokenize_chunk(strings.buf + strings.offsets[i], &tokens);
    }

    free_string_array(&strings);
    return tokens;
}
