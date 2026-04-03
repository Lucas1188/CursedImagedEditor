#ifndef PARSER_H
#define PARSER_H

#define MAX_ARG_LEN 256

/* Define the supported operations */
typedef enum {
    CMD_EMPTY,
    CMD_UNKNOWN,
    CMD_EXIT,
    CMD_LOAD,
    CMD_BLUR,
    CMD_LIST,
    CMD_INVERT,
    CMD_CLEAR,
    CMD_SELECT
} CommandType;

/* The 'AST' Node representing a parsed command */
typedef struct {
    CommandType type;
    char arg1[MAX_ARG_LEN];  /* Primary argument, e.g., filename for 'load' */
    char arg2[MAX_ARG_LEN];  /* Reserved for future multi-arg commands */
    int num_args;            /* How many arguments were successfully parsed */
} CommandAST;

/* Function prototypes */
CommandAST parse_command(const char* input);

#endif