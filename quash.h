#ifndef __QUASH_SHELL_H__
#define __QUASH_SHELL_H__

#include <stddef.h>
#include <sys/types.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define JOBS_MAX 256

typedef enum TokenEnum {
    T_NONE,                 /* default empty token */
    T_EOS,                  /* end of token stream */
    T_ERROR,                /* signals that an error occured during tokenizing */
    T_WORD,                 /* can be used as a command or as arguments to a command */
    T_GREATER,              /*  >  - used for redirecting STDOUT to a file */
    T_LESS,                 /*  <  - used for redirecting a file to STDIN  */
    T_GREATER_GREATER,      /*  >> - used for redirecting STDOUT to a file, appending */
    T_LESS_GREATER,         /*  <> - used for redirecting STDIN and STDOUT to the same file */
    T_GREATER_AMP,          /*  >& - used for redirecting STDERR to a file, or redirecting fds */
    T_GREATER_GREATER_AMP,  /* >>& - used for redirecting STDERR to a file, appending */
    T_PIPE,                 /*   | - used for piping STDOUT of left-hand to right-hand */
    T_AMP,                  /*   & - used for signaling to run the job asynchronously */
    T_AMP_AMP,              /* - unused - */
    T_PIPE_PIPE,            /* - unused - */
} TokenEnum;

typedef enum {
    TF_NUMBER                = 0x01,  /* token is suitable for use as a number */
    TF_DOUBLE_QUOTE_STRING   = 0x02,  /* token is a double-quoted string */
    TF_SINGLE_QUOTE_STRING   = 0x04,  /* token is a single-quoted string */
    TF_BACKTICK_QUOTE_STRING = 0x08,  /* token is a backtick-quoted string */
    TF_VARIABLE_NAME         = 0x10,  /* token is suitable for use as a variable name */
    TF_OPERATOR              = 0x20,  /* token is an operator */
} TokenFlags;

typedef struct Token {
    char *text;
    TokenEnum token:  16;
    TokenFlags flags: 16;
} Token;

typedef struct TokenDynamicArray {
    Token *tuples;
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
    char *fp;
    int fds[2];
    RedirectInstruction instr;
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


typedef int job_t;

enum JobFlags {
    JOB_RUNNING = 0x01,
    JOB_SUSPENDED = 0x02,
    JOB_ASYNC = 0x04,
};

typedef struct _Process {
    struct _Process *next;
    char *line;
    pid_t pid;
} Process;

typedef struct _Job {
    Process *processes;
    size_t process_count;
    int flags;
} Job;

#endif /* __QUASH_SHELL_H__ */