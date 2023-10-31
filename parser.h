#ifndef __QUASH_PARSER_H__
#define __QUASH_PARSER_H__

#include "arrays.h"
#include "quash.h"

ASTNode* parse_ast(TokenDynamicArray *tokens);
void print_parse_tree(ASTNode *tree);
void free_parse_tree(ASTNode *tree);
ASTNode* get_commands(ASTNode *ast);

#endif /* __QUASH_PARSER_H__ */
