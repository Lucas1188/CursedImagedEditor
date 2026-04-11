#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 8        /* Support up to 8 arguments (e.g., Triangle needs 6 coords + color) */
#define MAX_ARG_LEN 64    /* Max length of a single argument token */

typedef enum {
    CMD_EMPTY,
    CMD_UNKNOWN,
    CMD_EXIT,
    CMD_LOAD,
    CMD_LIST,
    CMD_SELECT,
    CMD_CLEAR,
    CMD_BLUR,
    CMD_INVERT,
    CMD_FILTER,
    CMD_NEW,
    CMD_DRAW,
    CMD_SAVE,
    CMD_EVAL,
    CMD_COLOR,
    CMD_HELP
} CommandType;

typedef struct {
    CommandType type;
    char args[MAX_ARGS][MAX_ARG_LEN];
    int num_args;
} CommandAST;

/* Core parser */
CommandAST parse_command(const char* input);

/* Common parsing helpers to extract types safely from the AST */
int get_arg_int(const CommandAST* ast, int index, int default_val);
const char* get_arg_str(const CommandAST* ast, int index, const char* default_val);

#endif