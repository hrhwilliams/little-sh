#include <stdlib.h>
#include <string.h>

#include "quash.h"
#include "arrays.h"


static void grow_string_offsets(StringDynamicBuffer *array) {
    array->strings_reserved *= 2;
    array->strings = realloc(array->strings, array->strings_reserved * sizeof *array->strings);
}

static void grow_string_buffer(StringDynamicBuffer *array) {
    array->buffer_reserved *= 2;
    array->buffer = realloc(array->buffer, array->buffer_reserved * sizeof *array->buffer);
}

void create_string_array(StringDynamicBuffer *array) {
    array->strings_reserved = STRING_DYNARRAY_DEFAULT_SIZE;
    array->strings_used = 0;
    array->strings = malloc(STRING_DYNARRAY_DEFAULT_SIZE * sizeof *array->strings);

    array->buffer_reserved = STRING_DYNARRAY_BUF_SIZE;
    array->buffer_used = 0;
    array->buffer = malloc(STRING_DYNARRAY_BUF_SIZE * sizeof *array->buffer);
}

void append_string(StringDynamicBuffer *array, char *string, size_t bytes) {
    if (!string) {
        array->strings[array->strings_used++] = array->buffer_used;
        array->buffer[array->buffer_used + 1] = '\0';
        array->buffer_used += 1;
        return;
    }

    size_t len = strlen(string);

    if (array->strings_used + 1 == array->strings_reserved) {
        grow_string_offsets(array);
    }

    while (array->buffer_used + len + 1 >= array->buffer_reserved) {
        grow_string_buffer(array);
    }

    array->strings[array->strings_used++] = array->buffer_used;

    if (bytes == 0 || bytes > len) {
        strncpy(array->buffer + array->buffer_used, string, len + 1);
    } else {
        strncpy(array->buffer + array->buffer_used, string, bytes + 1);
        (array->buffer + array->buffer_used)[bytes] = '\0';
    }

    array->buffer_used += len + 1;
}

void free_string_array(StringDynamicBuffer *array) {
    free(array->strings);
    free(array->buffer);
    memset(array, 0, sizeof *array);
}


static void grow_token_array(TokenDynamicArray *array) {
    array->slots *= 2;
    array->tuples = realloc(array->tuples, array->slots * sizeof *array->tuples);
}

void create_token_array(TokenDynamicArray *array) {
    array->slots = TOKEN_DYNARRAY_DEFAULT_SIZE;
    array->length = 0;
    array->tuples = malloc(TOKEN_DYNARRAY_DEFAULT_SIZE * sizeof *array->tuples);
}

void append_token(TokenDynamicArray *array, Token tuple) {
    if (array->length + 1 == array->slots) {
        grow_token_array(array);
    }

    array->tuples[array->length++] = tuple;
}

void free_token_array(TokenDynamicArray *array) {
    for (size_t i = 0; i < array->length; i++) {
        free(array->tuples[i].text);
    }

    free(array->tuples);
    memset(array, 0, sizeof *array);
}
