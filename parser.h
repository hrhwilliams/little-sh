#ifndef __QUASH_PARSER_H__
#define __QUASH_PARSER_H__

#include "arrays.h"
#include "quash.h"

Pipeline* parse(TokenDynamicArray *tokens);
void free_pipeline(Pipeline *pipeline);

#endif /* __QUASH_PARSER_H__ */