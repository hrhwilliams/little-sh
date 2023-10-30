#ifndef __QUASH_DYNAMIC_ARRAYS_H__
#define __QUASH_DYNAMIC_ARRAYS_H__

#include "quash.h"

#define TOKEN_DYNARRAY_DEFAULT_SIZE 8
#define STRING_DYNARRAY_DEFAULT_SIZE 8
#define STRING_DYNARRAY_BUF_SIZE 256

void create_string_array(StringDynamicBuffer *array);
void append_string(StringDynamicBuffer *array, char *string, size_t bytes);
void free_string_array(StringDynamicBuffer *array);

void create_token_array(TokenDynamicArray *array);
void append_token(TokenDynamicArray *array, Token tuple);
void free_token_array(TokenDynamicArray *array);

#endif /* __QUASH_DYNAMIC_ARRAYS_H__ */
