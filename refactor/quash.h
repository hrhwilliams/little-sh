#ifndef __QUASH_SHELL_H__
#define __QUASH_SHELL_H__

#include <stddef.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

typedef enum TokenEnum {
    T_NONE,
    T_EOF,
    T_ERROR,
    T_WORD,
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
    T_PIPE_PIPE,
} TokenEnum;

typedef struct TokenTuple {
    char *text;
    TokenEnum token;
} TokenTuple;

typedef struct TokenDynamicArray {
    TokenTuple *tuples;
    size_t length;
    size_t slots;
} TokenDynamicArray;


typedef struct StringDynamicBuffer{
    int *strings;
    char *buffer;
    size_t strings_used;
    size_t strings_reserved;
    size_t buffer_used;
    size_t buffer_reserved;
} StringDynamicBuffer;


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
    int asynchronous;
} Pipeline;

#endif /* __QUASH_SHELL_H__ */