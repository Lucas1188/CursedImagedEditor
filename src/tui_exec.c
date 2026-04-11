#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

#include "../include/tui_exec.h"
#include "../include/tui_state.h"
#include "../include/tui_math.h"
#include "../include/parser.h"
#include "../lib/bitmap/bitmap.h"
#include "../lib/bitmap/bitmap_cursed.h"
#include "../lib/png/png.h"
#include "../lib/cursedlib/math/kernels.h"

#define MAX_FILES 50
static char file_cache[MAX_FILES][256] = {0};
static int file_cache_count = 0;

/* Helper to extract a clean name (e.g., "./img/sprite.bmp" -> "sprite") */
static void extract_base_name(const char* filepath, char* out_name, size_t max_len) {
    const char* slash = strrchr(filepath, '/');
    const char* backslash = strrchr(filepath, '\\');
    const char* base = filepath;
    char* dot;

    /* Find the last folder separator */
    if (slash) base = slash + 1;
    if (backslash && backslash + 1 > base) base = backslash + 1;
    
    strncpy(out_name, base, max_len);
    out_name[max_len - 1] = '\0';
    
    /* Strip the file extension */
    dot = strrchr(out_name, '.');
    if (dot) *dot = '\0';
}

static void print_directory_list() {
    DIR *d;
    struct dirent *dir;
    file_cache_count = 0;

    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL && file_cache_count < MAX_FILES) {
            if (dir->d_name[0] == '.') continue;
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
    if (strlen(line_buf) > 0) add_log(line_buf);
    add_log("-----------------------");
}

/* ANSI C Compliant Helper: Centers an image inside the global canvas dimensions */
static cursed_img* fit_to_canvas(cursed_img* src, int target_w, int target_h) {
    cursed_img* dest;
    int offset_x, offset_y, y, x, dest_x, dest_y;
    
    dest = (cursed_img*)malloc(sizeof(cursed_img));
    if (!dest) return NULL;
    
    dest->width = target_w;
    dest->height = target_h;
    memcpy(&dest->px_fmt, &src->px_fmt, sizeof(src->px_fmt)); /* Safely copy the format struct */
    dest->pxs = (tcursed_pix*)malloc(target_w * target_h * sizeof(tcursed_pix));
    
    if (!dest->pxs) {
        free(dest);
        return NULL;
    }
    
    /* Wipe the new image completely transparent (0) */
    memset(dest->pxs, 0, target_w * target_h * sizeof(tcursed_pix));
    
    /* Calculate centering offsets (can be negative if source is larger than canvas) */
    offset_x = (target_w - src->width) / 2;
    offset_y = (target_h - src->height) / 2;
    
    /* Copy the pixels over */
    for (y = 0; y < src->height; y++) {
        for (x = 0; x < src->width; x++) {
            dest_x = x + offset_x;
            dest_y = y + offset_y;
            
            /* Only copy if the pixel falls within the canvas bounds. 
               This naturally handles both padding (smaller) and cropping (larger) */
            if (dest_x >= 0 && dest_x < target_w && dest_y >= 0 && dest_y < target_h) {
                dest->pxs[dest_y * target_w + dest_x] = src->pxs[y * src->width + x];
            }
        }
    }
    
    return dest;
}

int execute_command(CommandAST ast) {
    char msg_buffer[128];
    int i;

    switch (ast.type) {
        case CMD_EMPTY:
            return 1;
        case CMD_EXIT:
            return 0;
        case CMD_LIST:
            print_directory_list();
            break;
        case CMD_HELP:
            {
                const char* target = get_arg_str(&ast, 0, "");
                display_help(target);
            }
            break;
        case CMD_LOAD:
            {
                char target_file[256] = {0};
                int is_num = 1;
                const char* arg_str;
                cursed_img* loaded_img;
                
                if (ast.num_args < 1) {
                    print_directory_list();
                    add_log("Tip: Type 'load <number>' or 'load <filename>'");
                    return 1;
                }
                
                arg_str = get_arg_str(&ast, 0, "");
                for(i = 0; i < strlen(arg_str); i++) {
                    if(!isdigit(arg_str[i])) { is_num = 0; break; }
                }

                if (is_num) {
                    int idx = atoi(arg_str);
                    if (idx >= 0 && idx < file_cache_count) {
                        strncpy(target_file, file_cache[idx], 255);
                    } else {
                        add_log("Error: Invalid file index. Run 'list' to refresh.");
                        return 1;
                    }
                } else {
                    strncpy(target_file, arg_str, 255);
                }

                bitmap* temp_bmp = read_bitmap(target_file);
                if (!temp_bmp) {
                    snprintf(msg_buffer, sizeof(msg_buffer), "Error: Failed to read bitmap '%s'.", target_file);
                    add_log(msg_buffer);
                    return 1;
                }

                loaded_img = bitmap_to_cursed(temp_bmp);
                free_bitmap(temp_bmp); 

                if (!loaded_img) {
                    add_log("Error: Failed to convert bitmap to internal format.");
                    return 1;
                }

                
                if (canvas_width == 0 && canvas_height == 0) {
                    /* First image loaded sets the master canvas size */
                    canvas_width = loaded_img->width;
                    canvas_height = loaded_img->height;
                } else if (loaded_img->width != canvas_width || loaded_img->height != canvas_height) {
                    /* Image doesn't match canvas: Center and pad/crop it */
                    cursed_img* resized_img = fit_to_canvas(loaded_img, canvas_width, canvas_height);
                    
                    if (!resized_img) {
                        add_log("Error: Out of memory trying to fit image to canvas.");
                        RELEASE_CURSED_IMG(*loaded_img);
                        free(loaded_img);
                        return 1;
                    }
                    
                    /* Clean up the original raw image and point to the formatted one */
                    RELEASE_CURSED_IMG(*loaded_img);
                    free(loaded_img);
                    loaded_img = resized_img;
                    
                    snprintf(msg_buffer, sizeof(msg_buffer), "Notice: Image fitted to canvas (%dx%d)", canvas_width, canvas_height);
                    add_log(msg_buffer);
                }

                /* Inside CMD_LOAD layer assignment loop: */
                for (i = 0; i < MAX_LAYERS; i++) {
                    if (!layers[i].is_active) {
                        /* Extract a clean, math-friendly name */
                        char clean_name[64];
                        extract_base_name(target_file, clean_name, sizeof(clean_name));
                        
                        strncpy(layers[i].name, clean_name, 63); 
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
                RELEASE_CURSED_IMG(*loaded_img);
                free(loaded_img);
                add_log("Error: Max layers reached. Clear layers first.");
            }
            break;

        case CMD_SELECT:
            if (ast.num_args < 1) {
                add_log("Error: 'select' requires a layer name."); return 1;
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
            
        case CMD_INVERT:
            {
                if (selected_layer_idx == -1) { add_log("Error: No layer selected."); return 1; }
                int ident = layers[selected_layer_idx].op_count++;
                const char* op_name = (ast.type == CMD_BLUR) ? "BLUR" : "INVERT";
                snprintf(msg_buffer, sizeof(msg_buffer), "[%s | Ident %d] Executed %s", 
                         layers[selected_layer_idx].name, ident, op_name);
                add_log(msg_buffer);
            }
            break;
            
        case CMD_CLEAR:
            for (i = 0; i < MAX_LAYERS; i++) {
                if (layers[i].is_active && layers[i].img_data != NULL) {
                    RELEASE_CURSED_IMG(*(layers[i].img_data));
                    free(layers[i].img_data);
                }
            }
            memset(layers, 0, sizeof(layers));
            selected_layer_idx = -1;
            canvas_width = 0;  /* <-- Reset Canvas */
            canvas_height = 0; /* <-- Reset Canvas */
            add_log("-> Cleared all layers and reset canvas.");
            break;

        case CMD_NEW:
            {
                int w = 512, h = 512;
                char custom_name[64] = "Blank";

                /* Explicit argument counting for intuitive behavior */
                if (ast.num_args == 1) {
                    /* e.g., 'new shadow' */
                    if (canvas_width > 0 && canvas_height > 0) {
                        w = canvas_width; h = canvas_height;
                        strncpy(custom_name, get_arg_str(&ast, 0, "Blank"), 63);
                    } else {
                        add_log("Error: Canvas is empty. 'new' requires width and height (e.g., 'new 800 600').");
                        return 1;
                    }
                } else if (ast.num_args == 2) {
                    /* e.g., 'new 800 600' (Leaves name as "Blank") */
                    w = get_arg_int(&ast, 0, 512);
                    h = get_arg_int(&ast, 1, 512);
                } else if (ast.num_args >= 3) {
                    /* e.g., 'new 800 600 shadow' */
                    w = get_arg_int(&ast, 0, 512);
                    h = get_arg_int(&ast, 1, 512);
                    strncpy(custom_name, get_arg_str(&ast, 2, "Blank"), 63);
                }

                /* Canvas locking enforcement */
                if (canvas_width > 0 && canvas_height > 0) {
                    if (w != canvas_width || h != canvas_height) {
                        snprintf(msg_buffer, sizeof(msg_buffer), "Notice: Enforcing canvas size (%dx%d).", canvas_width, canvas_height);
                        add_log(msg_buffer);
                    }
                    w = canvas_width;
                    h = canvas_height;
                } else {
                    canvas_width = w;
                    canvas_height = h;
                }

                if (w <= 0 || h <= 0 || w > 8192 || h > 8192) {
                    add_log("Error: Dimensions must be between 1 and 8192."); 
                    return 1;
                }
                                
                int layer_slot = -1;
                for (i = 0; i < MAX_LAYERS; i++) {
                    if (!layers[i].is_active) { layer_slot = i; break; }
                }

                if (layer_slot == -1) { add_log("Error: Max layers reached."); return 1; }

                cursed_img* blank_img = (cursed_img*)malloc(sizeof(cursed_img));
                if (!blank_img) { add_log("Error: Failed to allocate memory."); return 1; }
                
                blank_img->width = w;
                blank_img->height = h;
                memcpy(&blank_img->px_fmt, &(spixel_fmt)CURSED_RGBA64_PXFMT, sizeof(spixel_fmt));
                blank_img->pxs = (tcursed_pix*)malloc(w * h * sizeof(tcursed_pix));

                if (blank_img->pxs == NULL) {
                    add_log("Error: Memory allocation for pixels failed.");
                    free(blank_img);
                    return 1;
                }

                memset(blank_img->pxs, 0, w * h * sizeof(tcursed_pix));

                strncpy(layers[layer_slot].name, custom_name, 63);
                layers[layer_slot].is_active = 1;
                layers[layer_slot].op_count = 0;
                layers[layer_slot].img_data = blank_img;
                selected_layer_idx = layer_slot;
                
                snprintf(msg_buffer, sizeof(msg_buffer), "-> Created new layer '%s' (%dx%d)", 
                         layers[layer_slot].name, w, h);
                add_log(msg_buffer);
            }
            break;

        case CMD_COLOR:
            {
                if (ast.num_args < 3) {
                    add_log("Error: 'color' requires r, g, b values (0-255). e.g., 'color 255 128 0'"); return 1;
                }
                int r = CLAMP(get_arg_int(&ast, 0, 255), 0, 255);
                int g = CLAMP(get_arg_int(&ast, 1, 255), 0, 255);
                int b = CLAMP(get_arg_int(&ast, 2, 255), 0, 255);
                int a = CLAMP(get_arg_int(&ast, 3, 255), 0, 255);
                current_color = make_pixel(r * 257, g * 257, b * 257, a * 257);
                snprintf(msg_buffer, sizeof(msg_buffer), "-> Set color to RGB(%d, %d, %d)", r, g, b);
                add_log(msg_buffer);
            }
            break;

        case CMD_DRAW:
            {
                const char* shape;
                cursed_img* target;

                if (selected_layer_idx == -1 || layers[selected_layer_idx].img_data == NULL) { 
                    add_log("Error: No active layer selected."); return 1; 
                }
                if (ast.num_args < 1) { 
                    add_log("Error: 'draw' requires a shape (e.g., 'draw circle ...')."); return 1; 
                }

                shape = get_arg_str(&ast, 0, "");
                target = layers[selected_layer_idx].img_data;

                if (strcmp(shape, "line") == 0) {
                    if (ast.num_args < 5) { add_log("Usage: draw line <x0> <y0> <x1> <y1>"); return 1; }
                    draw_line(get_arg_int(&ast, 1, 0), get_arg_int(&ast, 2, 0),
                              get_arg_int(&ast, 3, 0), get_arg_int(&ast, 4, 0), current_color, target);
                    add_log("-> Drew line.");
                } 
                else if (strcmp(shape, "rect") == 0 || strcmp(shape, "fillrect") == 0) {
                    if (ast.num_args < 5) { add_log("Usage: draw rect/fillrect <x0> <y0> <x1> <y1>"); return 1; }
                    if (strcmp(shape, "fillrect") == 0) {
                        fill_rectangle(get_arg_int(&ast, 1, 0), get_arg_int(&ast, 2, 0), 
                                       get_arg_int(&ast, 3, 0), get_arg_int(&ast, 4, 0), current_color, target);
                        add_log("-> Drew filled rectangle.");
                    } else {
                        draw_rectangle(get_arg_int(&ast, 1, 0), get_arg_int(&ast, 2, 0), 
                                       get_arg_int(&ast, 3, 0), get_arg_int(&ast, 4, 0), current_color, target);
                        add_log("-> Drew rectangle outline.");
                    }
                } 
                else if (strcmp(shape, "circle") == 0 || strcmp(shape, "fillcircle") == 0) {
                    if (ast.num_args < 4) { add_log("Usage: draw circle/fillcircle <cx> <cy> <r>"); return 1; }
                    if (strcmp(shape, "fillcircle") == 0) {
                        fill_circle(get_arg_int(&ast, 1, 0), get_arg_int(&ast, 2, 0), 
                                    get_arg_int(&ast, 3, 0), current_color, target);
                        add_log("-> Drew filled circle.");
                    } else {
                        draw_circle(get_arg_int(&ast, 1, 0), get_arg_int(&ast, 2, 0), 
                                    get_arg_int(&ast, 3, 0), current_color, target);
                        add_log("-> Drew circle outline.");
                    }
                } 
                else if (strcmp(shape, "triangle") == 0 || strcmp(shape, "filltriangle") == 0) {
                    if (ast.num_args < 7) { add_log("Usage: draw triangle/filltriangle <x0> y0 x1 y1 x2 y2>"); return 1; }
                    if (strcmp(shape, "filltriangle") == 0) {
                        fill_triangle(get_arg_int(&ast, 1, 0), get_arg_int(&ast, 2, 0), get_arg_int(&ast, 3, 0), 
                                      get_arg_int(&ast, 4, 0), get_arg_int(&ast, 5, 0), get_arg_int(&ast, 6, 0), current_color, target);
                        add_log("-> Drew filled triangle.");
                    } else {
                        draw_triangle(get_arg_int(&ast, 1, 0), get_arg_int(&ast, 2, 0), get_arg_int(&ast, 3, 0), 
                                      get_arg_int(&ast, 4, 0), get_arg_int(&ast, 5, 0), get_arg_int(&ast, 6, 0), current_color, target);
                        add_log("-> Drew triangle outline.");
                    }
                } 
                else {
                    add_log("Error: Unknown shape. Try: line, rect, circle, triangle");
                    add_log("       (add 'fill' prefix for solid shapes, e.g., 'fillcircle').");
                    return 1;
                }
                
                layers[selected_layer_idx].op_count++;
            }
            break;

        case CMD_SAVE:
            {
                if (selected_layer_idx == -1 || layers[selected_layer_idx].img_data == NULL) {
                    add_log("Error: No active layer selected to save."); return 1;
                }

                char format[16] = "png"; 
                char filename[256] = {0};
                const char* active_name = layers[selected_layer_idx].name;
                cursed_img* active_img = layers[selected_layer_idx].img_data;

                /* 1. Argument Parsing */
                if (ast.num_args == 0) {
                    snprintf(filename, sizeof(filename), "%s.png", active_name);
                } else if (ast.num_args == 1) {
                    const char* arg = get_arg_str(&ast, 0, "");
                    if (strcmp(arg, "png") == 0 || strcmp(arg, "bmp") == 0) {
                        strncpy(format, arg, 15);
                        snprintf(filename, sizeof(filename), "%s.%s", active_name, format);
                    } else {
                        strncpy(filename, arg, 255);
                        const char* dot = strrchr(filename, '.');
                        if (dot && strcmp(dot, ".bmp") == 0) {
                            strcpy(format, "bmp");
                        }
                    }
                } else {
                    strncpy(format, get_arg_str(&ast, 0, "png"), 15);
                    strncpy(filename, get_arg_str(&ast, 1, "output.png"), 255);
                }

                /* 2. Route to correct format logic */
                if (strcmp(format, "bmp") == 0) {
                    /* --- BMP EXPORT (8-bit) --- */
                    /* ONLY call cursed_to_bitmap if we are saving a BMP! */
                    bitmap* out_bmp = cursed_to_bitmap(active_img);
                    if (!out_bmp) { add_log("Error: Conversion to BMP failed."); return 1; }
                                        
                    if (write_bitmap(out_bmp, filename)) {
                        snprintf(msg_buffer, sizeof(msg_buffer), "-> Saved layer as 8-bit BMP to '%s'", filename);
                        add_log(msg_buffer);
                    } else {
                        add_log("Error: Failed to write BMP file.");
                    }
                    free_bitmap(out_bmp);
                    
               } else { 
                    /* --- PNG EXPORT (16-bit Truecolor) --- */
                    
                    size_t total_px = active_img->width * active_img->height;
                    size_t buf_size = total_px * 8; /* 4 channels * 2 bytes = 8 bytes per pixel */
                    size_t p;
                    /* Create a strict, PNG-compliant Big-Endian RGBA buffer */
                    unsigned char* png_buffer = (unsigned char*)malloc(buf_size);
                    if (!png_buffer) {
                        add_log("Error: Failed to allocate PNG export buffer.");
                        return 1;
                    }
                    
                    for (p = 0; p < total_px; p++) {
                        uint64_t px = active_img->pxs[p];
                        uint16_t r = UNPACK_R(px);
                        uint16_t g = UNPACK_G(px);
                        uint16_t b = UNPACK_B(px);
                        uint16_t a = UNPACK_A(px);
                        
                        /* PNG strictly requires Big-Endian (High byte, then Low byte) */
                        png_buffer[p*8 + 0] = (r >> 8) & 0xFF;
                        png_buffer[p*8 + 1] = r & 0xFF;
                        png_buffer[p*8 + 2] = (g >> 8) & 0xFF;
                        png_buffer[p*8 + 3] = g & 0xFF;
                        png_buffer[p*8 + 4] = (b >> 8) & 0xFF;
                        png_buffer[p*8 + 5] = b & 0xFF;
                        png_buffer[p*8 + 6] = (a >> 8) & 0xFF;
                        png_buffer[p*8 + 7] = a & 0xFF;
                    }
                    
                    ihdr_chunk ihdr = IHDR_TRUECOLOR16_A16(active_img->width, active_img->height);
                    png_s* png = create_png(&ihdr, png_buffer, 8); 
                    
                    if (png) {
                        if (write_png(filename, png) == 0) {
                            snprintf(msg_buffer, sizeof(msg_buffer), "-> Saved 16-bit layer as PNG to '%s'", filename);
                            add_log(msg_buffer);
                        } else add_log("Error: Failed to write PNG file.");
                        free_png(png);
                    } else add_log("Error: Failed to encode PNG structure.");
                    
                    free(png_buffer); /* Clean up the temporary export buffer */
                }
            }
            break;

        case CMD_EVAL:
            {
                char eval_str[256];
                char dest_name[64] = {0};
                char* eq_ptr;
                int p_idx = 0, j = 0;
                int dest_id;

                strncpy(eval_str, ast.args[0], 255); eval_str[255] = 0;

                eq_ptr = strchr(eval_str, '=');
                if (!eq_ptr) { add_log("Error: Missing '='."); return 1; }
                *eq_ptr = '\0'; 

                /* 1. Extract and trim the custom destination name */
                while (eval_str[p_idx] == ' ' || eval_str[p_idx] == '\t') p_idx++; 
                while (eval_str[p_idx] && eval_str[p_idx] != ' ' && eval_str[p_idx] != '\t') {
                    if (j < 63) dest_name[j++] = eval_str[p_idx];
                    p_idx++;
                }
                dest_name[j] = '\0';

                if (strlen(dest_name) == 0) { add_log("Error: No destination layer name."); return 1; }

                /* 2. Find existing layer or grab a new slot */
                dest_id = get_layer_idx_by_name(dest_name);
                if (dest_id == -1) {
                    for (i = 0; i < MAX_LAYERS; i++) {
                        if (!layers[i].is_active) { dest_id = i; break; }
                    }
                    if (dest_id == -1) { add_log("Error: Max layers reached."); return 1; }
                }

                /* 3. Initialize AST */
                ASTNode* root = init_and_parse_ast(eq_ptr + 1);
                if (!root) { add_log("Error: Syntax error or equation too complex."); return 1; }

                if (!check_ast_layers(root, &canvas_width, &canvas_height)) {
                    add_log("Error: Referenced layers missing or dimensions mismatch."); return 1;
                }

                /* 4. Prepare Destination Memory */
                cursed_img* dest_img = NULL;
                if (layers[dest_id].is_active) {
                    dest_img = layers[dest_id].img_data;
                } else {
                    dest_img = (cursed_img*)malloc(sizeof(cursed_img));
                    dest_img->width = canvas_width;
                    dest_img->height = canvas_height;
                    memcpy(&dest_img->px_fmt, &(spixel_fmt)CURSED_RGBA64_PXFMT, sizeof(spixel_fmt));
                    dest_img->pxs = (tcursed_pix*)malloc(canvas_width * canvas_height * sizeof(tcursed_pix));
                    
                    /* Wipe garbage RAM to pure transparency */
                    memset(dest_img->pxs, 0, canvas_width * canvas_height * sizeof(tcursed_pix));
                    
                    layers[dest_id].is_active = 1;
                    layers[dest_id].img_data = dest_img;
                    
                    /* Give it the exact name they typed! */
                    strncpy(layers[dest_id].name, dest_name, 63); 
                }
                layers[dest_id].op_count++;

                /* 5. Fire AST across all pixels */
                size_t total_pixels = canvas_width * canvas_height;
                size_t pp_idx;
                
                for (pp_idx = 0; pp_idx < total_pixels; pp_idx++) {
                    /* Evaluate the complete math tree for this specific pixel */
                    RGBFloat final_color = eval_ast(root, pp_idx);
                    
                    uint16_t r = CLAMP16((int)final_color.r, 0, 65535);
                    uint16_t g = CLAMP16((int)final_color.g, 0, 65535);
                    uint16_t b = CLAMP16((int)final_color.b, 0, 65535);
                    uint16_t a = CLAMP16((int)final_color.a, 0, 65535);
                    dest_img->pxs[pp_idx] = PACK_RGBA64(r, g, b, a);
                }
                selected_layer_idx = dest_id;
                add_log("-> Expression successfully evaluated.");
            }
            break;
        case CMD_FILTER:
            {
                const char* filter_name;
                
                if (selected_layer_idx == -1 || layers[selected_layer_idx].img_data == NULL) { 
                    add_log("Error: No active layer selected."); return 1; 
                }
                if (ast.num_args < 1) { 
                    add_log("Error: 'filter' requires a name (e.g., 'filter edge1' or 'filter blur 50')."); 
                    return 1; 
                }

                filter_name = get_arg_str(&ast, 0, "");

                /* --- ROUTE 1: Dynamic Separable Filters --- */
                if (strcmp(filter_name, "blur") == 0) {
                    int radius = get_arg_int(&ast, 1, 5);
                    double sigma = (double)get_arg_int(&ast, 2, 0);

                    if (radius > 1000) {
                        add_log("Notice: Clamping massive blur radius to 1000 to save CPU.");
                        radius = 1000;
                    }
                    
                    cursed_apply_separable_blur(layers[selected_layer_idx].img_data, radius, sigma);
                    
                    snprintf(msg_buffer, sizeof(msg_buffer), "-> Applied separable blur (Radius: %d)", radius);
                    add_log(msg_buffer);
                } 
                /* --- ROUTE 2: Hardcoded 3x3 and 5x5 Kernels --- */
                else {
                    const cursed_kernel* k = NULL;

                    if (strcmp(filter_name, "identity") == 0) k = &CURSED_KERNEL_IDENTITY;
                    else if (strcmp(filter_name, "edge1") == 0) k = &CURSED_KERNEL_EDGE1;
                    else if (strcmp(filter_name, "edge2") == 0) k = &CURSED_KERNEL_EDGE2;
                    else if (strcmp(filter_name, "sharpen") == 0) k = &CURSED_KERNEL_SHARPEN;
                    else if (strcmp(filter_name, "box_blur") == 0) k = &CURSED_KERNEL_BOX_BLUR;
                    else if (strcmp(filter_name, "gaussian3") == 0) k = &CURSED_KERNEL_GAUSSIAN3;
                    else if (strcmp(filter_name, "gaussian5") == 0) k = &CURSED_KERNEL_GAUSSIAN5;
                    else if (strcmp(filter_name, "unsharp5") == 0) k = &CURSED_KERNEL_UNSHARP5;
                    else if (strcmp(filter_name, "emboss") == 0) k = &CURSED_KERNEL_EMBOSS;
                    else if (strcmp(filter_name, "sobel_x") == 0) k = &CURSED_KERNEL_SOBEL_GX;
                    else if (strcmp(filter_name, "sobel_y") == 0) k = &CURSED_KERNEL_SOBEL_GY;

                    if (k) {
                        cursed_apply_kernel(layers[selected_layer_idx].img_data, k);
                        snprintf(msg_buffer, sizeof(msg_buffer), "-> Applied 2D kernel '%s'", filter_name);
                        add_log(msg_buffer);
                    } else {
                        add_log("Error: Unknown filter.");
                        add_log("       Try: blur <rad>, edge1, sharpen, emboss, sobel_x, etc.");
                        return 1;
                    }
                }
                
                layers[selected_layer_idx].op_count++;
            }
            break;
        case CMD_UNKNOWN:
        default:
            add_log("Error: Unknown internal command.");
            break;
    }
    return 1;
}