#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>
#include <glob.h>

typedef struct _Process {
    char **p_argv;
    char *input_fp;
    char *output_fp;
    int p_argc;
    int input_fd;
    int output_fd;
    int append;
} Process;

typedef struct _Job {
    Process *processes;
} Job;

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

// command < file.txt > out.txt 2> error.txt
/*
 * WordList := WORD
 *           | WORD WordList
 *
 * Command := WordList
 *          | WordList RedirectionList
 *
 * RedirectionList := Redirection*
 *
 * Redirection := '>' WORD        -> redirect STDOUT to FILENAME
 *              | '<' WORD        -> redirect FILENAME to STDIN
 *              | NUMBER '>' WORD -> redirect FD to FILENAME
 *              | NUMBER '<' WORD -> redirect FILENAME to FD
 *              | '>>' WORD -> redirect FD to FILENAME, appending
 *              | NUMBER '>>' WORD -> redirect FD to FILENAME, appending
 *              | '<>' WORD -> redirect FILENAME to SDTIN, and STDOUT to FILENAME
 *              | '&>' WORD -> redirect STDOUT and STDERR to FILENAME
 *              | '&>>' WORD -> redirect STDOUT and STDERR to FILENAME, appending
 *              | '>&' WORD -> redirect STDOUT and STDERR to FILENAME
 *              | '>&' NUMBER -> redirect STDOUT to FD
 *              | '>>&' WORD -> redirect STDOUT and STDERR to FILENAME, appending
 * 
 * Pipe := Command '|' Command -> pipe stdout of $1 into stdin of $2
 *       | Command '|&' Command -> pipe stdout and stderr of $1 into stdin of $2
 *       | Command
 * 
 * Conditional := Pipe
 *              | Pipe '&&' Conditional  -> only execute right-hand side if left-hand side returns 0, and returns 0
 *              | Pipe '||' Conditional  -> execute right-hand side if left-hand side returns nonzero, and returns 1
 */

typedef struct TokenTuple {
    char *text;
    Token token;
} TokenTuple;

typedef struct TokenDynamicArray {
    TokenTuple *tuples;
    size_t length;
    size_t slots;
} TokenDynamicArray;

typedef struct StringDynamicArray{
    int *offsets;
    char *buf;
    size_t length;
    size_t slots;
    size_t buf_length;
    size_t buf_slots;
} StringDynamicArray;

void grow_token_array(TokenDynamicArray *array) {
    array->slots *= 2;
    array->tuples = realloc(array->tuples, array->slots * sizeof *array->tuples);
}

void grow_string_array(StringDynamicArray *array) {
    array->slots *= 2;
    array->offsets = realloc(array->offsets, array->slots * sizeof *array->offsets);
}

void grow_string_buf(StringDynamicArray *array) {
    array->buf_length *= 2;
    array->buf = realloc(array->buf, array->buf_length * sizeof *array->buf);
}

#define DYNARRAY_DEFAULT_SIZE 8
#define STRING_DYNARRAY_BUF_SIZE 256

void create_token_array(TokenDynamicArray *array) {
    array->slots = DYNARRAY_DEFAULT_SIZE;
    array->length = 0;
    array->tuples = malloc(DYNARRAY_DEFAULT_SIZE * sizeof *array->tuples);
}

void create_string_array(StringDynamicArray *array) {
    array->slots = DYNARRAY_DEFAULT_SIZE;
    array->length = 0;
    array->offsets = malloc(DYNARRAY_DEFAULT_SIZE * sizeof *array->offsets);

    array->buf_slots = STRING_DYNARRAY_BUF_SIZE;
    array->buf_length = 0;
    array->buf = malloc(STRING_DYNARRAY_BUF_SIZE * sizeof *array->buf);
}

void append_token(TokenDynamicArray *array, TokenTuple tuple) {
    if (array->length + 1 == array->slots) {
        grow_token_array(array);
    }

    array->tuples[array->length++] = tuple;
}

void append_string(StringDynamicArray *array, char *string, int bytes) {
    size_t len = strlen(string);

    if (array->length + 1 == array->slots) {
        grow_string_array(array);
    }

    if (array->buf_length + len + 1 >= array->buf_slots) {
        grow_string_buf(array);
    }

    array->offsets[array->length++] = array->buf_length;

    if (bytes == -1) {
        strncpy(array->buf + array->buf_length, string, len + 1);
    } else {
        strncpy(array->buf + array->buf_length, string, bytes);
    }
    
    array->buf_length += len + 1;
}

void free_token_array(TokenDynamicArray *array) {
    free(array->tuples);
}

void free_string_array(StringDynamicArray *array) {
    free(array->offsets);
    free(array->buf);
}

int is_whitespace(char c) {
    return c == ' ' || c == '\t';
}

int is_quote_char(char c) {
    return c == '\'' || c == '\"' || c == '`';
}

int is_number(char c) {
    return c >= '0' && c <= '9';
}

int is_escape_char(char c) {
    return c == '\\';
}

int is_delimiter(char c) {
    return c == ' ' || c == '\n' || c == '\t';
}

int is_var_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||is_number(c) || c == '_';
}

int is_word_char(char c) {
    return is_var_char(c) || c == '.' || c == '/';
}

int is_metachar(char c) {
    switch (c) {
    case '|':
    case '&':
    case ';':
    case '<':
    case '>':
    case '(':
    case ')':
    case ' ':
    case '\t':
    case '\n':
        return 1;
    default:
        return 0;
    }
}

void tokenize_metachar(char *input, TokenDynamicArray *tokens, int *index) {
    switch (input[0]) {
    case '|':
        if (input[1] == '|') {
            append_token(tokens, (TokenTuple) { NULL, T_PIPE_PIPE }); *index += 2 ;return;
        } else if (input[1] == '&') {
            append_token(tokens, (TokenTuple) { NULL, T_PIPE_AMP }); *index += 2 ;return;
        } else {
            append_token(tokens, (TokenTuple) { NULL, T_PIPE }); *index += 1 ; return;
        }
        break;
    case '&':
        if (input[1] == '&') {
            append_token(tokens, (TokenTuple) { NULL, T_AMP_AMP }); *index += 2; return;
        } else {
            append_token(tokens, (TokenTuple) { NULL, T_AMP }); *index += 1; return;
        }
        break;
    case '<':
        if (input[1] == '<') {
            append_token(tokens, (TokenTuple) { NULL, T_LESS_LESS }); *index += 2; return;
        } else if (input[1] == '>') {
            append_token(tokens, (TokenTuple) { NULL, T_LESS_GREATER }); *index += 2; return;
        } else {
            append_token(tokens, (TokenTuple) { NULL, T_LESS }); *index += 1; return;
        }
        break;
    case '>':
        if (input[1] == '>') {
            if (input[2] == '&') {
                append_token(tokens, (TokenTuple) { NULL, T_GREATER_GREATER_AMP }); *index += 3; return;
            } else {
                append_token(tokens, (TokenTuple) { NULL, T_GREATER_GREATER }); *index += 2; return;
            }
        } else if (input[1] == '&') {
            append_token(tokens, (TokenTuple) { NULL, T_GREATER_AMP }); *index += 2; return;
        } else {
            append_token(tokens, (TokenTuple) { NULL, T_GREATER }); *index += 1; return;
        } 
    break;
    }

    append_token(tokens, (TokenTuple) { input, T_ERROR });
    *index += 1;
}

void tokenize_variable(char *string, TokenDynamicArray *tokens, int *index) {
    TokenTuple ret;

    for (int i = 0;; i++) {
        if (!is_var_char(string[i])) {
            ret.token = T_VARIABLE;
            ret.text = strndup(string, i);
            append_token(tokens, ret);
            return;
        }
    }
}

void tokenize_string(char *string, TokenDynamicArray *tokens, int *index) {
    TokenTuple ret;
    char quote_char = string[0];

    int i;
    for (i = 1;; i++) {
        if (string[i] == '\0') {
            ret.text = NULL;
            ret.token = T_ERROR;
            *index += i;
            break;
        }
        if (string[i] == quote_char) {
            ret.text = strndup(string + 1, i - 2);
            ret.token = T_STRING;
            *index += i;
            break;
        }
    }

    append_token(tokens, ret);
}

int tokenize_number(char *input, TokenDynamicArray *tokens, int *index) {
    TokenTuple ret;

    for (int i = 0;; i++) {
        if (input[i] == '\0') {
            ret.text = strndup(input, i);
            ret.token = T_NUMBER;
            append_token(tokens, ret);
            *index += i - 1;
            return 1;
        }

        if (!is_number(input[i])) {
            return 0;
        }
    }
}

void tokenize_word(char *string, TokenDynamicArray *tokens, int *index) {
    TokenTuple ret;

    for (int i = 0;; i++) {
        if (string[i] == '\0' || is_whitespace(string[i]) || is_quote_char(string[i]) || is_metachar(string[i])) {
            ret.token = T_WORD;
            ret.text = strndup(string, i);
            *index += i - 1;
            append_token(tokens, ret);
            return;
        }
    }
}

void tokenize_chunk(char *input, TokenDynamicArray *tokens) {
    for (int i = 0; input[i] != '\0'; i++) {
        char c = input[i];

        if (is_escape_char(c)) {
            continue;
        }

        if (is_whitespace(c)) {
            return;
        }

        if (c == '$') {
            tokenize_variable(input + i, tokens, &i);
        } else if (is_number(c) && tokenize_number(input + i, tokens, &i)) {
        } else if (is_metachar(c)) {
            tokenize_metachar(input + i, tokens, &i);
        } else if (is_quote_char(c)) {
            tokenize_string(input + i, tokens, &i);
        } else {
            tokenize_word(input + i, tokens, &i);
        }
    }
}

TokenDynamicArray tokenize(StringDynamicArray *input) {
    TokenDynamicArray tokens;
    create_token_array(&tokens);

    for (int i = 0; i < input->length; i++) {
        tokenize_chunk(input->buf + input->offsets[i], &tokens);
    }

    return tokens;
}

/*
 * at this point it might be best to implement strtok myself to do this
 */
StringDynamicArray expand_globs(char *input) {
    const char *delim = " \t\n";

    glob_t globbuf;
    memset(&globbuf, 0, sizeof globbuf);

    StringDynamicArray strings;
    create_string_array(&strings);

    int glob_flags = GLOB_NOSORT | GLOB_NOCHECK | GLOB_NOMAGIC;

    int word_start = 0;
    for (int i = 0; input[i] != '\0'; i++) {
        if (is_metachar(input[i])) {
            append_string(&strings, input + word_start, i - word_start);
            word_start = i;
        }

        if (is_quote_char(input[i])) {
            char endquote = input[i];

            /* parse until endquote encountered */
        }
    }

#if 0
    while(token != NULL) {
        int rc;
        // need to do something to preserve quotes here
        if (token[0] == '\"') {
            int len1 = strlen(token);

            // put entire quote in list of char vectors then do next token
            char *token2 = strtok(NULL, "\"");

            if (token2) {
                int len2 = strlen(token2);
                char buf[len1 + len2 + 2];
                strcpy(buf, token);
                buf[len1] = ' ';
                strcat(buf, token2);
                append_string(&strings, strdup(buf));
                // printf("-string-%s-string\n", buf);
            } else {
                token[len1 - 1] = '\0';
                append_string(&strings, token);
            }

            token = token2;
            token = strtok(NULL, delim);
        } else {
            rc = glob(token, flags, NULL, &globbuf);

            if (rc != 0) {
                printf("glob: %d oops\n", rc);
            }

            for (int i = 0; i < globbuf.gl_pathc; i++) {
                append_string(&strings, globbuf.gl_pathv[i]);
            }

            token = strtok(NULL, delim);
        }
    }

    if (globbuf.gl_pathc > 0) {
        globfree(&globbuf);
    }
#endif
    return strings;
}

int interactive_prompt() {
    const char *prompt = "$ ";
    char *line;
    StringDynamicArray strings;
    TokenDynamicArray tokens;

    for (;;) {
        
        line = readline(prompt);

        if (!line) {
            break;
        }

        if (*line) {
            add_history(line);
        }

        strings = expand_globs(line);
        tokens = tokenize(&strings);

        for (int i = 0; i < strings.length; i++) {
            printf("(%s) ", strings.buf + strings.offsets[i]);
        }
        printf("\n");

        for (int i = 0; i < tokens.length; i++) {
            printf("(%d, %s) ", tokens.tuples[i].token, tokens.tuples[i].text);
        }
        printf("\n");

        // for (int i = 0; i < globbuf.gl_pathc; i++) {
        //     printf("%s ", globbuf.gl_pathv[i]);
        // }
        // printf("\n");

        free(line);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    interactive_prompt();

    return 0;
}
