#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>   /* For directory reading */
#include <ctype.h>    /* For isdigit */
#include "../include/parser.h"
#include "../lib/bitmap/bitmap.h"
#include "../lib/bitmap/bitmap_cursed.h"
#include "../lib/cursedlib/image/image.h"
#include "../lib/png/png.h"

#define MAX_LAYERS 4
#define MAX_CMD_LEN 256
#define VIEWPORT_LINES 12 /* Height of the scrollable log window */
#define MAX_HISTORY 100

tcursed_pix current_color; /* Tracks the active drawing color */

void (*cursed_log_callback)(const char* msg) = NULL;

/* --- STATE MANAGEMENT --- */
typedef struct {
    char name[64];
    int is_active;
    int op_count;
    cursed_img* img_data; /* <--- Holds the loaded pixel data */
} Layer;

Layer layers[MAX_LAYERS] = {0};
int selected_layer_idx = -1; /* -1 means no layer is currently selected */

/* Rolling log buffer for the interaction window */
char window_log[MAX_HISTORY][128] = {0};
int log_count = 0;

/* --- HELPER FUNCTIONS --- */
void clear_screen() {
    printf("\033[2J\033[H");
}

/* Adds a formatted message to the scrolling interaction window */
void add_log(const char* message) {
    int i;
    if (log_count < MAX_HISTORY) {
        strncpy(window_log[log_count], message, 127);
        log_count++;
    } else {
        /* Shift everything up by 1 if we hit the history limit */
        for (i = 1; i < MAX_HISTORY; i++) {
            strcpy(window_log[i-1], window_log[i]);
        }
        strncpy(window_log[MAX_HISTORY-1], message, 127);
    }
}

/* --- TUI RENDERING --- */
void draw_ui() {
    int i;
    clear_screen();
    
    /* 1. HEADER (Layers) */
    printf("\033[1;37;44m CURSED IMG EDTR v0.1 | LAYERS: ");
    for (i = 0; i < MAX_LAYERS; i++) {
        if (layers[i].is_active) {
            if (i == selected_layer_idx) {
                printf("\033[1;33m[*%s* (Ops:%d)]\033[1;37;44m ", layers[i].name, layers[i].op_count);
            } else {
                printf("[%s] ", layers[i].name);
            }
        } else {
            printf("[Empty] ");
        }
    }
    
    /* NEW: Convert 16-bit internal color back to 8-bit for the ANSI Terminal */
    uint16_t* c = (uint16_t*)&current_color;
    int r8 = c[0] / 257; 
    int g8 = c[1] / 257; 
    int b8 = c[2] / 257;
    
    /* Print the active color block using ANSI Truecolor, then reset formatting */
    printf(" | COLOR: \033[48;2;%d;%d;%dm    \033[0m\n", r8, g8, b8);
    printf("--------------------------------------------------------------------------------\n");
    
    /* 2. INTERACTION WINDOW (Scrollable Log) */
    int start_idx = (log_count > VIEWPORT_LINES) ? (log_count - VIEWPORT_LINES) : 0;
    
    for (i = 0; i < VIEWPORT_LINES; i++) {
        if (start_idx + i < log_count) {
            printf(" %s\n", window_log[start_idx + i]);
        } else {
            printf("\n"); /* Print empty lines to maintain window height */
        }
    }
    
    /* 3. FOOTER (Help Info) */
    printf("--------------------------------------------------------------------------------\n");
    printf("\033[90m CMD : load <n/name>, ls, save [fmt], new [w] [h], select <n>, clear, exit \033[0m\n");
    printf("\033[90m FX  : blur, invert, rect | MATH: l<id> = [w]*l<id> +/- ... (e.g. l2=0.5*l1+l0) \033[0m\n");
}

#define MAX_FILES 50
char file_cache[MAX_FILES][256] = {0};
int file_cache_count = 0;

/* Helper to read directory and print to the interaction window */
void print_directory_list() {
    DIR *d;
    struct dirent *dir;
    file_cache_count = 0;

    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL && file_cache_count < MAX_FILES) {
            if (dir->d_name[0] == '.') continue; /* Skip hidden files and ./.. */
            strncpy(file_cache[file_cache_count], dir->d_name, 255);
            file_cache_count++;
        }
        closedir(d);
    }

    if (file_cache_count == 0) {
        add_log("No files found in the current directory.");
        return;
    }

    add_log("--- Directory Files ---");
    
    /* Format multiple files per line so we don't instantly flush the viewport */
    char line_buf[128] = {0};
    int i;
    for (i = 0; i < file_cache_count; i++) {
        char item[64];
        snprintf(item, sizeof(item), "[%d] %-15s ", i, file_cache[i]);
        
        if (strlen(line_buf) + strlen(item) >= 110) {
            add_log(line_buf);
            memset(line_buf, 0, sizeof(line_buf));
        }
        strcat(line_buf, item);
    }
    if (strlen(line_buf) > 0) {
        add_log(line_buf);
    }
    add_log("-----------------------");
}

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
/* 16-bit unsigned integers max out at 65535 (0xFFFF) */
#define CLAMP16(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

/* Unpack a 64-bit pixel into 16-bit channels (Assuming R-G-B-A layout) */
#define UNPACK_R(px) (((px) >> 48) & 0xFFFF)
#define UNPACK_G(px) (((px) >> 32) & 0xFFFF)
#define UNPACK_B(px) (((px) >> 16) & 0xFFFF)
#define UNPACK_A(px) ((px) & 0xFFFF)

/* Pack four 16-bit channels back into a single 64-bit uint64_t */
#define PACK_RGBA64(r, g, b, a) \
    (((uint64_t)(r) << 48) | ((uint64_t)(g) << 32) | ((uint64_t)(b) << 16) | (uint64_t)(a))

#define MAX_EVAL_TERMS 8 /* Maximum number of layers you can chain in one equation */

/* Represents one parsed chunk of the equation, e.g., "-0.5*l2" */
typedef struct {
    int layer_idx;
    float weight; 
} EvalTerm;

/* Helper to find a layer index either by its number ID or its string name */
int resolve_layer_idx(const char* arg) {
    int i;
    if (strlen(arg) == 0) return -1;
    
    /* Check if argument is purely a number */
    int is_num = 1;
    for(i = 0; i < strlen(arg); i++) {
        if(!isdigit(arg[i])) { is_num = 0; break; }
    }
    
    if (is_num) {
        int idx = atoi(arg);
        if (idx >= 0 && idx < MAX_LAYERS && layers[idx].is_active) return idx;
    } else {
        /* Fallback: Search by name */
        for (i = 0; i < MAX_LAYERS; i++) {
            if (layers[i].is_active && strcmp(layers[i].name, arg) == 0) return i;
        }
    }
    return -1;
}

/* --- MATH AST ENGINE --- */
typedef enum { AST_OP_ADD, AST_OP_SUB, AST_OP_MUL, AST_OP_DIV, AST_LAYER, AST_CONST } ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    float value; /* Holds layer ID or constant float */
    struct ASTNode* left;
    struct ASTNode* right;
} ASTNode;

/* Static memory pool for blazing fast AST allocation (no malloc/free needed) */
ASTNode ast_pool[256];
int ast_node_count = 0;
const char* parse_ptr;

static ASTNode* make_node(ASTNodeType type) {
    if (ast_node_count >= 256) return NULL;
    ASTNode* n = &ast_pool[ast_node_count++];
    n->type = type; n->left = n->right = NULL; n->value = 0;
    return n;
}

void ast_skip_space() { while (*parse_ptr == ' ' || *parse_ptr == '\t') parse_ptr++; }
ASTNode* parse_ast_expr();

ASTNode* parse_ast_factor() {
    ast_skip_space();
    ASTNode* n = NULL;

    if (*parse_ptr == '(') {
        parse_ptr++;
        n = parse_ast_expr();
        ast_skip_space();
        if (*parse_ptr == ')') parse_ptr++;
    } else if (*parse_ptr == '-') { /* Handle Unary Minus (e.g. -l1 or -0.5) */
        parse_ptr++; ast_skip_space();
        n = make_node(AST_OP_SUB);
        ASTNode* zero = make_node(AST_CONST); zero->value = 0.0f;
        n->left = zero; n->right = parse_ast_factor();
    } else if (*parse_ptr == 'l') { /* Handle Layer (e.g. l1) */
        parse_ptr++;
        n = make_node(AST_LAYER);
        n->value = atof(parse_ptr);
        while (*parse_ptr >= '0' && *parse_ptr <= '9') parse_ptr++;
    } else { /* Handle Constant (e.g. 0.5) */
        n = make_node(AST_CONST);
        n->value = atof(parse_ptr);
        while ((*parse_ptr >= '0' && *parse_ptr <= '9') || *parse_ptr == '.') parse_ptr++;
    }
    return n;
}

ASTNode* parse_ast_term() {
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

ASTNode* parse_ast_expr() {
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

/* Validates that all referenced layers exist and match dimensions */
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

typedef struct { float r, g, b; } RGBFloat;

/* Evaluates the AST for a single pixel location recursively */
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
/* --- END MATH AST ENGINE --- */

/* ANSI C compliant packed pixel (immune to Strict Aliasing crashes) */
tcursed_pix make_pixel(uint16_t r, uint16_t g, uint16_t b, uint16_t a) {
    uint64_t px = 0;
    px |= ((uint64_t)r);
    px |= ((uint64_t)g << 16);
    px |= ((uint64_t)b << 32);
    px |= ((uint64_t)a << 48);
    return (tcursed_pix)px;
}

/* --- EXECUTOR --- */
int execute_command(CommandAST ast) {
    char msg_buffer[128];
    int i; /* Declare once at the top to be used across all cases */

    switch (ast.type) {
        case CMD_EMPTY:
            return 1;
            
        case CMD_EXIT:
            return 0;
            
        case CMD_LIST:
            print_directory_list();
            break;

        case CMD_LOAD:
            {
                char target_file[256] = {0};
                int is_num = 1;
                
                /* 1. If no args, just list the files and hint the user */
                if (ast.num_args < 1) {
                    print_directory_list();
                    add_log("Tip: Type 'load <number>' or 'load <filename>'");
                    return 1;
                }

                /* Safely fetch the first argument as a string */
                const char* arg_str = get_arg_str(&ast, 0, "");

                /* 2. Check if the argument is purely digits */
                for(i = 0; i < strlen(arg_str); i++) {
                    if(!isdigit(arg_str[i])) { 
                        is_num = 0; 
                        break; 
                    }
                }

                /* 3. Resolve the target file string */
                if (is_num) {
                    int idx = atoi(arg_str);
                    if (idx >= 0 && idx < file_cache_count) {
                        strncpy(target_file, file_cache[idx], 255);
                    } else {
                        add_log("Error: Invalid file index. Run 'list' to refresh.");
                        return 1;
                    }
                } else {
                    /* It's not a number, assume they typed the literal filename */
                    strncpy(target_file, arg_str, 255);
                }

                /* 4. Try to read the bitmap from disk */
                bitmap* temp_bmp = read_bitmap(target_file);
                if (!temp_bmp) {
                    snprintf(msg_buffer, sizeof(msg_buffer), "Error: Failed to read bitmap '%s'.", target_file);
                    add_log(msg_buffer);
                    return 1;
                }

                /* 5. Convert to your internal cursed format */
                cursed_img* loaded_img = bitmap_to_cursed(temp_bmp);
                free_bitmap(temp_bmp); 

                if (!loaded_img) {
                    add_log("Error: Failed to convert bitmap to internal format.");
                    return 1;
                }

                /* 6. Load it into the next available UI Layer */
                for (i = 0; i < MAX_LAYERS; i++) {
                    if (!layers[i].is_active) {
                        /* Optionally use extract_layer_name() here if you added it */
                        strncpy(layers[i].name, target_file, 63); 
                        layers[i].is_active = 1;
                        layers[i].op_count = 0;
                        layers[i].img_data = loaded_img; 
                        
                        selected_layer_idx = i;
                        
                        snprintf(msg_buffer, sizeof(msg_buffer), "-> Loaded '%s' (Size: %dx%d)", 
                                 layers[i].name, loaded_img->width, loaded_img->height);
                        add_log(msg_buffer);
                        return 1;
                    }
                }
                
                /* 7. If layers are full */
                RELEASE_CURSED_IMG(*loaded_img);
                free(loaded_img);
                add_log("Error: Max layers reached. Clear layers first.");
            }
            break;

        case CMD_SELECT:
            if (ast.num_args < 1) {
                add_log("Error: 'select' requires a layer name.");
                return 1;
            }
            
            const char* target_name = get_arg_str(&ast, 0, "");
            
            for (i = 0; i < MAX_LAYERS; i++) {
                if (layers[i].is_active && strcmp(layers[i].name, target_name) == 0) {
                    selected_layer_idx = i;
                    snprintf(msg_buffer, sizeof(msg_buffer), "-> Selected layer '%s'", layers[i].name);
                    add_log(msg_buffer);
                    return 1;
                }
            }
            add_log("Error: Layer not found.");
            break;
            
        case CMD_BLUR:
        case CMD_INVERT:
            {
                int ident;
                const char* op_name;

                if (selected_layer_idx == -1) {
                    add_log("Error: No layer selected. Use 'select <name>' first.");
                    return 1;
                }
                
                /* Get current ident, then increment */
                ident = layers[selected_layer_idx].op_count++;
                op_name = (ast.type == CMD_BLUR) ? "BLUR" : "INVERT";
                
                snprintf(msg_buffer, sizeof(msg_buffer), "[%s | Ident %d] Executed %s", 
                         layers[selected_layer_idx].name, ident, op_name);
                add_log(msg_buffer);
            }
            break;
            
        case CMD_CLEAR:
            for (i = 0; i < MAX_LAYERS; i++) {
                if (layers[i].is_active && layers[i].img_data != NULL) {
                    /* Free the underlying cursed_img data safely */
                    RELEASE_CURSED_IMG(*(layers[i].img_data));
                    free(layers[i].img_data);
                }
            }
            /* Now it's safe to wipe the struct array */
            memset(layers, 0, sizeof(layers));
            selected_layer_idx = -1;
            add_log("-> Cleared all layers and freed memory.");
            break;
        case CMD_NEW:
            {
                /* 1. Extract arguments */
                int w = get_arg_int(&ast, 0, 512);
                int h = get_arg_int(&ast, 1, 512);
                const char* custom_name = get_arg_str(&ast, 2, "Blank");

                if (w <= 0 || h <= 0 || w > 8192 || h > 8192) {
                    add_log("Error: Dimensions must be between 1 and 8192.");
                    return 1;
                }

                /* 2. Find an empty layer slot */
                int layer_slot = -1;
                int i;
                for (i = 0; i < MAX_LAYERS; i++) {
                    if (!layers[i].is_active) {
                        layer_slot = i;
                        break;
                    }
                }

                if (layer_slot == -1) {
                    add_log("Error: Max layers reached. Clear layers first.");
                    return 1;
                }

                /* 3. Allocate the image struct */
                cursed_img* blank_img = (cursed_img*)malloc(sizeof(cursed_img));
                if (!blank_img) {
                    add_log("Error: Failed to allocate memory for image struct.");
                    return 1;
                }
                
                blank_img->width = w;
                blank_img->height = h;
                memcpy(&blank_img->px_fmt, &(spixel_fmt)CURSED_RGBA64_PXFMT, sizeof(spixel_fmt));
                blank_img->pxs = (tcursed_pix*)malloc(w * h * sizeof(tcursed_pix));

                if (blank_img->pxs == NULL) {
                    add_log("Error: Memory allocation for pixels failed.");
                    free(blank_img);
                    return 1;
                }

                /* 4. WIPE THE GARBAGE MEMORY! */
                memset(blank_img->pxs, 0, w * h * sizeof(tcursed_pix));

                if (blank_img->pxs == NULL) {
                    add_log("Error: Memory allocation for pixels failed.");
                    return 1;
                }

                /* 4. WIPE THE GARBAGE MEMORY! 
                   Set all bytes to 0 so the layer is actually blank/transparent */
                memset(blank_img->pxs, 0, w * h * sizeof(tcursed_pix));

                /* 5. Assign to UI state */
                strncpy(layers[layer_slot].name, custom_name, 63);
                layers[layer_slot].is_active = 1;
                layers[layer_slot].op_count = 0;
                layers[layer_slot].img_data = blank_img;
                
                selected_layer_idx = layer_slot;
                
                char msg_buffer[128];
                snprintf(msg_buffer, sizeof(msg_buffer), "-> Created new layer '%s' (%dx%d)", 
                         layers[layer_slot].name, w, h);
                add_log(msg_buffer);
            }
            break;

        case CMD_COLOR:
            {
                if (ast.num_args < 3) {
                    add_log("Error: 'color' requires r, g, b values (0-255). e.g., 'color 255 128 0'");
                    return 1;
                }
                
                int r = get_arg_int(&ast, 0, 255);
                int g = get_arg_int(&ast, 1, 255);
                int b = get_arg_int(&ast, 2, 255);
                int a = get_arg_int(&ast, 3, 255); /* Alpha is optional, defaults to opaque */

                /* Clamp to standard 8-bit ranges */
                r = CLAMP(r, 0, 255);
                g = CLAMP(g, 0, 255);
                b = CLAMP(b, 0, 255);
                a = CLAMP(a, 0, 255);

                /* Scale 8-bit input to 16-bit engine constraints (val * 257) */
                current_color = make_pixel(r * 257, g * 257, b * 257, a * 257);
                
                snprintf(msg_buffer, sizeof(msg_buffer), "-> Set color to RGB(%d, %d, %d)", r, g, b);
                add_log(msg_buffer);
            }
            break;

        case CMD_RECT:
            {
                if (selected_layer_idx == -1) {
                    add_log("Error: Select a layer first."); return 1;
                }
                
                if (layers[selected_layer_idx].img_data->pxs == NULL) {
                    add_log("Error: Image pixel buffer is NULL!"); return 1;
                }

                int x0 = get_arg_int(&ast, 0, 0);
                int y0 = get_arg_int(&ast, 1, 0);
                int x1 = get_arg_int(&ast, 2, 100);
                int y1 = get_arg_int(&ast, 3, 100);
                
                if (x0 > x1) { int tmp = x0; x0 = x1; x1 = tmp; }
                if (y0 > y1) { int tmp = y0; y0 = y1; y1 = tmp; }

                size_t max_w = layers[selected_layer_idx].img_data->width - 1;
                size_t max_h = layers[selected_layer_idx].img_data->height - 1;
                
                if (x1 > max_w) x1 = max_w;
                if (y1 > max_h) y1 = max_h;
                
                /* DELETED: The hardcoded red color */
                /* CHANGED: Pass the global current_color to the drawing function */
                draw_rectangle(x0, y0, x1, y1, current_color, layers[selected_layer_idx].img_data);
                
                add_log("-> Drew rectangle.");
            }
            break;
        case CMD_SAVE:
            {
                if (selected_layer_idx == -1 || layers[selected_layer_idx].img_data == NULL) {
                    add_log("Error: No active layer selected to save.");
                    return 1;
                }

                char format[16] = "png"; /* Default format */
                char filename[256] = {0};

                /* Parse arguments based on user input */
                if (ast.num_args == 0) {
                    /* 0 args: default to png, use layer 0 name (or selected layer if layer 0 is empty) */
                    int name_idx = layers[0].is_active ? 0 : selected_layer_idx;
                    snprintf(filename, sizeof(filename), "%s.png", layers[name_idx].name);
                } 
                else if (ast.num_args == 1) {
                    /* 1 arg: default to png, use provided filename */
                    strncpy(filename, get_arg_str(&ast, 0, "output.png"), 255);
                } 
                else {
                    /* 2+ args: format provided, filename provided */
                    strncpy(format, get_arg_str(&ast, 0, "png"), 15);
                    strncpy(filename, get_arg_str(&ast, 1, "output.png"), 255);
                }

                /* 1. Convert the internal format back to a standard bitmap for saving */
                cursed_img* target_img = layers[selected_layer_idx].img_data;
                bitmap* out_bmp = cursed_to_bitmap(target_img);
                
                if (!out_bmp) {
                    add_log("Error: Failed to convert layer for saving.");
                    return 1;
                }

                /* 2. Route to the correct file writer */
                if (strcmp(format, "bmp") == 0) {
                    if (write_bitmap(out_bmp, filename)) { /* Assumes write_bitmap returns non-zero on success */
                        snprintf(msg_buffer, sizeof(msg_buffer), "-> Saved layer as BMP to '%s'", filename);
                        add_log(msg_buffer);
                    } else {
                        add_log("Error: Failed to write BMP file.");
                    }
                } 
                else { /* Default to PNG */
                    ihdr_chunk ihdr = IHDR_TRUECOLOR8_A8(out_bmp->width, out_bmp->height);
                    /* Assuming pixels are 4-bytes (RGBA) as per your original driver code */
                    png_s* png = create_png(&ihdr, out_bmp->pixels, 4); 
                    
                    if (png) {
                        if (write_png(filename, png) == 0) { /* Assumes write_png returns 0 on success */
                            snprintf(msg_buffer, sizeof(msg_buffer), "-> Saved layer as PNG to '%s'", filename);
                            add_log(msg_buffer);
                        } else {
                            add_log("Error: Failed to write PNG file.");
                        }
                        free_png(png);
                    } else {
                        add_log("Error: Failed to encode PNG structure.");
                    }
                }

                /* 3. Clean up the intermediate bitmap */
                free_bitmap(out_bmp);
            }
            break;
        case CMD_EVAL:
            {
                char eval_str[256];
                strncpy(eval_str, ast.args[0], 255);
                eval_str[255] = 0;

                char* eq_ptr = strchr(eval_str, '=');
                if (!eq_ptr) { add_log("Error: Missing '='."); return 1; }
                *eq_ptr = '\0'; /* Terminate string at '=' to separate destination from equation */

                int dest_id;
                if (sscanf(eval_str, " l%d", &dest_id) != 1 || dest_id < 0 || dest_id >= MAX_LAYERS) {
                    add_log("Error: Invalid destination layer format (e.g., 'l3')."); return 1;
                }

                /* Initialize the Parser */
                ast_node_count = 0;       /* Reset the memory pool */
                parse_ptr = eq_ptr + 1;   /* Point parser to the right side of the '=' */
                ASTNode* root = parse_ast_expr();

                if (!root) {
                    add_log("Error: Syntax error or equation too complex."); return 1;
                }

                /* Validate all layers mapped in the AST */
                int target_w = -1, target_h = -1;
                if (!check_ast_layers(root, &target_w, &target_h)) {
                    add_log("Error: Referenced layers missing or dimensions do not match."); return 1;
                }
                if (target_w == -1) {
                    add_log("Error: No layers referenced in expression."); return 1;
                }

                /* Prepare the Destination Layer */
                cursed_img* dest_img = NULL;
                if (layers[dest_id].is_active) {
                    dest_img = layers[dest_id].img_data;
                    if (dest_img->width != target_w || dest_img->height != target_h) {
                        add_log("Error: Destination dimensions do not match sources."); return 1;
                    }
                } else {
                    dest_img = &GEN_CURSED_IMG(target_w, target_h);
                    layers[dest_id].is_active = 1;
                    layers[dest_id].img_data = dest_img;
                    snprintf(layers[dest_id].name, 63, "Eval_Result");
                }
                layers[dest_id].op_count++;

                /* Fire the AST Engine across all pixels */
                size_t total_pixels = target_w * target_h;
                size_t p_idx;
                
                for (p_idx = 0; p_idx < total_pixels; p_idx++) {
                    /* Evaluate the complete math tree for this specific pixel */
                    RGBFloat final_color = eval_ast(root, p_idx);
                    
                    /* Map directly to the 64-bit array via 16-bit pointers to avoid Endianness bugs */
                    uint16_t* dest_channels = (uint16_t*)&(dest_img->pxs[p_idx]);
                    dest_channels[0] = CLAMP16((int)final_color.r, 0, 65535);
                    dest_channels[1] = CLAMP16((int)final_color.g, 0, 65535);
                    dest_channels[2] = CLAMP16((int)final_color.b, 0, 65535);
                    dest_channels[3] = 65535; /* Force Opaque Alpha */
                }

                selected_layer_idx = dest_id;
                add_log("-> Expression successfully evaluated.");
            }
            break;
        case CMD_UNKNOWN:
        default:
            add_log("Error: Unknown internal command.");
            break;
    }
    
    return 1;
}

/* --- MAIN INTERACTIVE LOOP --- */
int interactive_mode() {
    char input_buffer[MAX_CMD_LEN];
    cursed_log_callback = add_log; /* Route all backend logs into the UI window */
    current_color = make_pixel(65535, 0, 0, 65535);
    add_log("Welcome to CURSED-IE.");
    
    while(1) {
        draw_ui();
        
        printf("\n> ");
        if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
            break; 
        }
        
        input_buffer[strcspn(input_buffer, "\n")] = 0; 
        
        /* Echo the user's raw input into the log for context */
        if (strlen(input_buffer) > 0) {
            char echo[128];
            snprintf(echo, sizeof(echo), "> %s", input_buffer);
            add_log(echo);
        }
        
        CommandAST current_cmd = parse_command(input_buffer);
        int should_continue = execute_command(current_cmd);
        
        if (!should_continue) {
            break;
        }
    }
    
    clear_screen();
    return 1;
}