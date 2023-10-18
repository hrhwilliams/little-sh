#include <stdlib.h>
#include <string.h>

#include "string_buf.h"

static void grow_string_offsets(StringDynamicBuffer *array);
static void grow_string_buffer(StringDynamicBuffer *array);

void create_string_array(StringDynamicBuffer *array) {
    array->strings_reserved = STRING_DYNARRAY_DEFAULT_SIZE;
    array->strings_used = 0;
    array->strings = malloc(STRING_DYNARRAY_DEFAULT_SIZE * sizeof *array->strings);

    array->buffer_reserved = STRING_DYNARRAY_BUF_SIZE;
    array->buffer_used = 0;
    array->buffer = malloc(STRING_DYNARRAY_BUF_SIZE * sizeof *array->buffer);
}

void append_string(StringDynamicBuffer *array, char *string, int bytes) {
    size_t len = strlen(string);

    if (array->strings_used + 1 == array->strings_reserved) {
        grow_string_offsets(array);
    }

    while (array->buffer_used + len + 1 >= array->buffer_reserved) {
        grow_string_buffer(array);
    }

    array->strings[array->strings_used++] = array->buffer_used;

    if (bytes == -1) {
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
}

static void grow_string_offsets(StringDynamicBuffer *array) {
    array->strings_reserved *= 2;
    array->strings = realloc(array->strings, array->strings_reserved * sizeof *array->strings);
}

static void grow_string_buffer(StringDynamicBuffer *array) {
    array->buffer_reserved *= 2;
    array->buffer = realloc(array->buffer, array->buffer_reserved * sizeof *array->buffer);
}
