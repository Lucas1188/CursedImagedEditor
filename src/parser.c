#include <stdio.h>
#include <string.h>
#include "../include/parser.h"

CommandAST parse_command(const char* input) {
    CommandAST ast = { .type = CMD_UNKNOWN, .arg1 = {0}, .arg2 = {0}, .num_args = 0 };
    char cmd_str[MAX_ARG_LEN] = {0};

    /* Extract the command and up to two arguments */
    int scanned = sscanf(input, "%255s %255s %255s", cmd_str, ast.arg1, ast.arg2);

    if (scanned <= 0) {
        ast.type = CMD_EMPTY;
        return ast;
    }

    ast.num_args = scanned - 1; /* Subtract 1 because the command itself isn't an arg */

    /* Map string tokens to CommandTypes */
    if (strcmp(cmd_str, "exit") == 0) {
        ast.type = CMD_EXIT;
    } 
    else if (strcmp(cmd_str, "list") == 0 || strcmp(cmd_str, "ls") == 0) {
        ast.type = CMD_LIST;
    }
    else if (strcmp(cmd_str, "load") == 0) {
        ast.type = CMD_LOAD;
    } 
    else if (strcmp(cmd_str, "blur") == 0) {
        ast.type = CMD_BLUR;
    } 
    else if (strcmp(cmd_str, "invert") == 0) {
        ast.type = CMD_INVERT; 
    } 
    else if (strcmp(cmd_str, "clear") == 0) {
        ast.type = CMD_CLEAR;
    }
    else if (strcmp(cmd_str, "select") == 0) {
        ast.type = CMD_SELECT;
    }

    return ast;
}