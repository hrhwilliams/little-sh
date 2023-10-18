#ifndef __QUASH_STRING_BUFFER_H__
#define __QUASH_STRING_BUFFER_H__

#include <stddef.h>

#define STRING_DYNARRAY_DEFAULT_SIZE 8
#define STRING_DYNARRAY_BUF_SIZE 256

typedef struct StringDynamicBuffer{
    int *strings;
    char *buffer;
    size_t strings_used;
    size_t strings_reserved;
    size_t buffer_used;
    size_t buffer_reserved;
} StringDynamicBuffer;

void create_string_array(StringDynamicBuffer *array);
void append_string(StringDynamicBuffer *array, char *string, int bytes);
void free_string_array(StringDynamicBuffer *array);

#endif /* __QUASH_STRING_BUFFER_H__ */