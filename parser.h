#ifndef __QUASH_PARSER_H__
#define __QUASH_PARSER_H__

#include "string_buf.h"
#include "tokenizer.h"

typedef enum {
    RI_READ_FILE,         /* cmd  < file */
    RI_WRITE_FILE,        /* cmd  > file */
    RI_WRITE_APPEND_FILE, /* cmd >> file */
    RI_READ_WRITE_FILE,   /* cmd <> file */
    RI_REDIRECT_FD        /* cmd $1 >& $2*/
} RedirectInstruction;

/*
 linked-list of redirects for the command
*/
typedef struct _Redirect {
    struct _Redirect *next;
    RedirectInstruction instr;
    union {
        int fds[2];
        char *fp;
    };
} Redirect;

/*
 linked-list of commands
*/
typedef struct _Command {
    Redirect *redirects;
    struct _Command *next;
    StringDynamicBuffer strings;
    char **argv;
    int argc;
    int flags; 
} Command;

typedef struct _Pipeline {
    Command *commands;
    int count;
} Pipeline;

Pipeline* eval(TokenDynamicArray *tokens);

#endif /* __QUASH_PARSER_H__ */