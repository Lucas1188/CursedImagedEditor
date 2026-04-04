#include <stdlib.h>
#include "../include/tui_math.h"

/* Static internals only visible to the math engine */
static ASTNode ast_pool[256];
static int ast_node_count = 0;
static const char* parse_ptr;

static ASTNode* make_node(ASTNodeType type) {
    if (ast_node_count >= 256) return NULL;
    ASTNode* n = &ast_pool[ast_node_count++];
    n->type = type; n->left = n->right = NULL; n->value = 0;
    return n;
}

static void ast_skip_space() { while (*parse_ptr == ' ' || *parse_ptr == '\t') parse_ptr++; }
static ASTNode* parse_ast_expr();

static ASTNode* parse_ast_factor() {
    ast_skip_space();
    ASTNode* n = NULL;

    if (*parse_ptr == '(') {
        parse_ptr++;
        n = parse_ast_expr();
        ast_skip_space();
        if (*parse_ptr == ')') parse_ptr++;
    } else if (*parse_ptr == '-') {
        parse_ptr++; ast_skip_space();
        n = make_node(AST_OP_SUB);
        ASTNode* zero = make_node(AST_CONST); zero->value = 0.0f;
        n->left = zero; n->right = parse_ast_factor();
    } else if (*parse_ptr == 'l') {
        parse_ptr++;
        n = make_node(AST_LAYER);
        n->value = atof(parse_ptr);
        while (*parse_ptr >= '0' && *parse_ptr <= '9') parse_ptr++;
    } else {
        n = make_node(AST_CONST);
        n->value = atof(parse_ptr);
        while ((*parse_ptr >= '0' && *parse_ptr <= '9') || *parse_ptr == '.') parse_ptr++;
    }
    return n;
}

static ASTNode* parse_ast_term() {
    ASTNode* node = parse_ast_factor();
    ast_skip_space();
    while (*parse_ptr == '*' || *parse_ptr == '/') {
        char op = *parse_ptr++;
        ASTNode* right = parse_ast_factor();
        ASTNode* parent = make_node(op == '*' ? AST_OP_MUL : AST_OP_DIV);
        parent->left = node; parent->right = right;
        node = parent;
        ast_skip_space();
    }
    return node;
}

static ASTNode* parse_ast_expr() {
    ASTNode* node = parse_ast_term();
    ast_skip_space();
    while (*parse_ptr == '+' || *parse_ptr == '-') {
        char op = *parse_ptr++;
        ASTNode* right = parse_ast_term();
        ASTNode* parent = make_node(op == '+' ? AST_OP_ADD : AST_OP_SUB);
        parent->left = node; parent->right = right;
        node = parent;
        ast_skip_space();
    }
    return node;
}

ASTNode* init_and_parse_ast(const char* rhs_string) {
    ast_node_count = 0;
    parse_ptr = rhs_string;
    return parse_ast_expr();
}

int check_ast_layers(ASTNode* node, int* w, int* h) {
    if (!node) return 1;
    if (node->type == AST_LAYER) {
        int s_id = (int)node->value;
        if (s_id < 0 || s_id >= MAX_LAYERS || !layers[s_id].is_active) return 0;
        if (*w == -1) {
            *w = layers[s_id].img_data->width;
            *h = layers[s_id].img_data->height;
        } else if (*w != layers[s_id].img_data->width || *h != layers[s_id].img_data->height) {
            return 0;
        }
    }
    return check_ast_layers(node->left, w, h) && check_ast_layers(node->right, w, h);
}

RGBFloat eval_ast(ASTNode* node, size_t p_idx) {
    RGBFloat res = {0,0,0};
    if (!node) return res;
    if (node->type == AST_CONST) {
        res.r = res.g = res.b = node->value; return res;
    }
    if (node->type == AST_LAYER) {
        int l_id = (int)node->value;
        uint16_t* ch = (uint16_t*)&(layers[l_id].img_data->pxs[p_idx]);
        res.r = ch[0]; res.g = ch[1]; res.b = ch[2];
        return res;
    }
    
    RGBFloat L = eval_ast(node->left, p_idx);
    RGBFloat R = eval_ast(node->right, p_idx);
    
    switch (node->type) {
        case AST_OP_ADD: res.r=L.r+R.r; res.g=L.g+R.g; res.b=L.b+R.b; break;
        case AST_OP_SUB: res.r=L.r-R.r; res.g=L.g-R.g; res.b=L.b-R.b; break;
        case AST_OP_MUL: res.r=L.r*R.r; res.g=L.g*R.g; res.b=L.b*R.b; break;
        case AST_OP_DIV:
            res.r = (R.r == 0) ? 65535 : L.r / R.r;
            res.g = (R.g == 0) ? 65535 : L.g / R.g;
            res.b = (R.b == 0) ? 65535 : L.b / R.b;
            break;
    }
    return res;
}