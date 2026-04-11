#include <stdlib.h>
#include <string.h>
#include "../include/tui_math.h"

/* Static internals only visible to the math engine */
static ASTNode ast_pool[256];
static int ast_node_count = 0;
static const char* parse_ptr;

static ASTNode* make_node(ASTNodeType type) {
    if (ast_node_count >= 256) return NULL;
    ASTNode* n = &ast_pool[ast_node_count++];
    n->type = type; n->left = n->right = NULL; n->value = 0; n->channel_mask = 15;
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
    } else {
        char token[64] = {0};
        int i = 0;
        int mask = 15; /* Default to 15 (1|2|4|8), meaning all channels active */
        char *endptr, *bracket, *c;
        double val;
        
        while (*parse_ptr && *parse_ptr != ' ' && *parse_ptr != '\t' && 
               *parse_ptr != '+' && *parse_ptr != '-' && 
               *parse_ptr != '*' && *parse_ptr != '/' && 
               *parse_ptr != '(' && *parse_ptr != ')') {
            if (i < 63) token[i++] = *parse_ptr;
            parse_ptr++;
        }
        
        /* --- NEW: CHANNEL MASK DETECTION --- */
        bracket = strchr(token, '[');
        if (bracket) {
            *bracket = '\0'; /* Cut the string here so 'token' becomes just the layer name */
            mask = 0;        /* Reset mask, we will build it from the brackets */
            c = bracket + 1;
            while (*c && *c != ']') {
                if (*c == 'r' || *c == 'R') mask |= 1;
                else if (*c == 'g' || *c == 'G') mask |= 2;
                else if (*c == 'b' || *c == 'B') mask |= 4;
                else if (*c == 'a' || *c == 'A') mask |= 8;
                c++;
            }
            if (mask == 0) mask = 15; /* Safe fallback */
        }
        
        val = strtod(token, &endptr);
        
        if (*endptr == '\0' && strlen(token) > 0) {
            n = make_node(AST_CONST);
            n->value = (float)val;
        } else {
            n = make_node(AST_LAYER);
            n->value = (float)get_layer_idx_by_name(token); 
            n->channel_mask = mask; /* Attach the mask to the node! */
        }
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
    RGBFloat res = {0.0f, 0.0f, 0.0f, 0.0f};
    
    if (!node) return res;

    if (node->type == AST_CONST) {
        /* Broadcast scalar to all channels */
        res.r = res.g = res.b = res.a = node->value;
        return res;
    }

    if (node->type == AST_LAYER) {
        int l_id = (int)node->value;
        uint64_t px = layers[l_id].img_data->pxs[p_idx];
        
        /* Apply mask: If the bit is missing, the channel stays 0.0f */
        if (node->channel_mask & 1) res.r = (float)UNPACK_R(px);
        if (node->channel_mask & 2) res.g = (float)UNPACK_G(px);
        if (node->channel_mask & 4) res.b = (float)UNPACK_B(px);
        if (node->channel_mask & 8) res.a = (float)UNPACK_A(px);
        
        return res;
    }

    RGBFloat L = eval_ast(node->left, p_idx);
    RGBFloat R = eval_ast(node->right, p_idx);

    switch (node->type) {
        case AST_OP_ADD: 
            res.r = L.r + R.r; res.g = L.g + R.g; res.b = L.b + R.b; res.a = L.a + R.a; break;
        case AST_OP_SUB: 
            res.r = L.r - R.r; res.g = L.g - R.g; res.b = L.b - R.b; res.a = L.a - R.a; break;
        case AST_OP_MUL: 
            res.r = L.r * R.r; res.g = L.g * R.g; res.b = L.b * R.b; res.a = L.a * R.a; break;
        case AST_OP_DIV: 
            res.r = (R.r != 0) ? L.r / R.r : 0; 
            res.g = (R.g != 0) ? L.g / R.g : 0; 
            res.b = (R.b != 0) ? L.b / R.b : 0; 
            res.a = (R.a != 0) ? L.a / R.a : 0; 
            break;
    }
    return res;
}