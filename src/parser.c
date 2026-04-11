#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/parser.h"

CommandAST parse_command(const char* input) {
    CommandAST ast;
    memset(&ast, 0, sizeof(CommandAST));
    ast.type = CMD_UNKNOWN;

    /* Make a mutable copy of the input for strtok */
    char buffer[256];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    /* NEW: Intercept algebra expressions (anything with an '=' sign) */
    if (strchr(buffer, '=') != NULL) {
        ast.type = CMD_EVAL;
        strncpy(ast.args[0], buffer, MAX_ARG_LEN - 1);
        ast.num_args = 1;
        return ast;
    }

    /* Get the main command token */
    char* token = strtok(buffer, " \t\r\n");
    if (!token) {
        ast.type = CMD_EMPTY;
        return ast;
    }

    char cmd_str[MAX_ARG_LEN];
    strncpy(cmd_str, token, sizeof(cmd_str) - 1);

    /* Parse all subsequent arguments into the args array */
    while ((token = strtok(NULL, " \t\r\n")) != NULL && ast.num_args < MAX_ARGS) {
        strncpy(ast.args[ast.num_args], token, MAX_ARG_LEN - 1);
        ast.num_args++;
    }

    /* Map command string to CommandType */
    if (strcmp(cmd_str, "exit") == 0) ast.type = CMD_EXIT;
    else if (strcmp(cmd_str, "load") == 0) ast.type = CMD_LOAD;
    else if (strcmp(cmd_str, "list") == 0 || strcmp(cmd_str, "ls") == 0) ast.type = CMD_LIST;
    else if (strcmp(cmd_str, "select") == 0) ast.type = CMD_SELECT;
    else if (strcmp(cmd_str, "clear") == 0) ast.type = CMD_CLEAR;
    else if (strcmp(cmd_str, "blur") == 0) ast.type = CMD_BLUR;
    else if (strcmp(cmd_str, "invert") == 0) ast.type = CMD_INVERT;
    else if (strcmp(cmd_str, "new") == 0) ast.type = CMD_NEW;
    else if (strcmp(cmd_str, "save") == 0) ast.type = CMD_SAVE;
    else if (strcmp(cmd_str, "draw") == 0) ast.type = CMD_DRAW; 
    else if (strcmp(cmd_str, "color") == 0) ast.type = CMD_COLOR;
    else if (strcmp(cmd_str, "filter") == 0) ast.type = CMD_FILTER;
    else if (strcmp(cmd_str, "help") == 0) ast.type = CMD_HELP;

    return ast;
}

/* Helper: Safely grabs an integer from the args list, or returns the default */
int get_arg_int(const CommandAST* ast, int index, int default_val) {
    if (index < 0 || index >= ast->num_args) return default_val;
    return atoi(ast->args[index]);
}

/* Helper: Safely grabs a string from the args list, or returns the default */
const char* get_arg_str(const CommandAST* ast, int index, const char* default_val) {
    if (index < 0 || index >= ast->num_args) return default_val;
    return ast->args[index];
}