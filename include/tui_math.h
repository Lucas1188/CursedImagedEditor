#ifndef TUI_MATH_H
#define TUI_MATH_H

#include <stddef.h>
#include "tui_state.h"

typedef struct { float r, g, b; } RGBFloat;

typedef enum { AST_OP_ADD, AST_OP_SUB, AST_OP_MUL, AST_OP_DIV, AST_LAYER, AST_CONST } ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    float value;
    struct ASTNode* left;
    struct ASTNode* right;
} ASTNode;

/* Encapsulated API for the executor to call */
ASTNode* init_and_parse_ast(const char* rhs_string);
int check_ast_layers(ASTNode* node, int* w, int* h);
RGBFloat eval_ast(ASTNode* node, size_t p_idx);

#endif