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
/* IMPORTANT: Include your drawing header so draw_rectangle is defined! */
/* #include "../lib/cursedlib/image/draw/draw.h" */

#define MAX_FILES 50
static char file_cache[MAX_FILES][256] = {0};
static int file_cache_count = 0;

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

        case CMD_LOAD:
            {
                char target_file[256] = {0};
                int is_num = 1;
                
                if (ast.num_args < 1) {
                    print_directory_list();
                    add_log("Tip: Type 'load <number>' or 'load <filename>'");
                    return 1;
                }
                const char* arg_str = get_arg_str(&ast, 0, "");
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

                cursed_img* loaded_img = bitmap_to_cursed(temp_bmp);
                free_bitmap(temp_bmp); 

                if (!loaded_img) {
                    add_log("Error: Failed to convert bitmap to internal format.");
                    return 1;
                }

                for (i = 0; i < MAX_LAYERS; i++) {
                    if (!layers[i].is_active) {
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
            
        case CMD_BLUR:
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
            add_log("-> Cleared all layers and freed memory.");
            break;

        case CMD_NEW:
            {
                int w = get_arg_int(&ast, 0, 512);
                int h = get_arg_int(&ast, 1, 512);
                const char* custom_name = get_arg_str(&ast, 2, "Blank");

                if (w <= 0 || h <= 0 || w > 8192 || h > 8192) {
                    add_log("Error: Dimensions must be between 1 and 8192."); return 1;
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

        case CMD_RECT:
            {
                if (selected_layer_idx == -1) { add_log("Error: Select a layer first."); return 1; }
                if (layers[selected_layer_idx].img_data->pxs == NULL) { add_log("Error: Pixel buffer is NULL!"); return 1; }

                int x0 = get_arg_int(&ast, 0, 0); int y0 = get_arg_int(&ast, 1, 0);
                int x1 = get_arg_int(&ast, 2, 100); int y1 = get_arg_int(&ast, 3, 100);
                
                if (x0 > x1) { int tmp = x0; x0 = x1; x1 = tmp; }
                if (y0 > y1) { int tmp = y0; y0 = y1; y1 = tmp; }

                size_t max_w = layers[selected_layer_idx].img_data->width - 1;
                size_t max_h = layers[selected_layer_idx].img_data->height - 1;
                
                if (x1 > max_w) x1 = max_w;
                if (y1 > max_h) y1 = max_h;
                
                draw_rectangle(x0, y0, x1, y1, current_color, layers[selected_layer_idx].img_data);
                add_log("-> Drew rectangle.");
            }
            break;

        case CMD_SAVE:
            {
                if (selected_layer_idx == -1 || layers[selected_layer_idx].img_data == NULL) {
                    add_log("Error: No active layer selected to save."); return 1;
                }

                char format[16] = "png"; 
                char filename[256] = {0};

                if (ast.num_args == 0) {
                    int name_idx = layers[0].is_active ? 0 : selected_layer_idx;
                    snprintf(filename, sizeof(filename), "%s.png", layers[name_idx].name);
                } else if (ast.num_args == 1) {
                    strncpy(filename, get_arg_str(&ast, 0, "output.png"), 255);
                } else {
                    strncpy(format, get_arg_str(&ast, 0, "png"), 15);
                    strncpy(filename, get_arg_str(&ast, 1, "output.png"), 255);
                }

                bitmap* out_bmp = cursed_to_bitmap(layers[selected_layer_idx].img_data);
                if (!out_bmp) { add_log("Error: Conversion failed."); return 1; }

                if (strcmp(format, "bmp") == 0) {
                    if (write_bitmap(out_bmp, filename)) {
                        snprintf(msg_buffer, sizeof(msg_buffer), "-> Saved layer as BMP to '%s'", filename);
                        add_log(msg_buffer);
                    } else add_log("Error: Failed to write BMP file.");
                } else { 
                    ihdr_chunk ihdr = IHDR_TRUECOLOR8_A8(out_bmp->width, out_bmp->height);
                    png_s* png = create_png(&ihdr, out_bmp->pixels, 4); 
                    if (png) {
                        if (write_png(filename, png) == 0) {
                            snprintf(msg_buffer, sizeof(msg_buffer), "-> Saved layer as PNG to '%s'", filename);
                            add_log(msg_buffer);
                        } else add_log("Error: Failed to write PNG file.");
                        free_png(png);
                    } else add_log("Error: Failed to encode PNG structure.");
                }
                free_bitmap(out_bmp);
            }
            break;

        case CMD_EVAL:
            {
                char eval_str[256];
                strncpy(eval_str, ast.args[0], 255); eval_str[255] = 0;

                char* eq_ptr = strchr(eval_str, '=');
                if (!eq_ptr) { add_log("Error: Missing '='."); return 1; }
                *eq_ptr = '\0'; 

                int dest_id;
                if (sscanf(eval_str, " l%d", &dest_id) != 1 || dest_id < 0 || dest_id >= MAX_LAYERS) {
                    add_log("Error: Invalid destination layer format (e.g., 'l3')."); return 1;
                }

                ASTNode* root = init_and_parse_ast(eq_ptr + 1);
                if (!root) { add_log("Error: Syntax error or equation too complex."); return 1; }

                int target_w = -1, target_h = -1;
                if (!check_ast_layers(root, &target_w, &target_h)) {
                    add_log("Error: Referenced layers missing or dimensions do not match."); return 1;
                }
                if (target_w == -1) { add_log("Error: No layers referenced."); return 1; }

                cursed_img* dest_img = NULL;
                if (layers[dest_id].is_active) {
                    dest_img = layers[dest_id].img_data;
                    if (dest_img->width != target_w || dest_img->height != target_h) {
                        add_log("Error: Destination dimensions do not match sources."); return 1;
                    }
                } else {
                    dest_img = (cursed_img*)malloc(sizeof(cursed_img));
                    dest_img->width = target_w;
                    dest_img->height = target_h;
                    memcpy(&dest_img->px_fmt, &(spixel_fmt)CURSED_RGBA64_PXFMT, sizeof(spixel_fmt));
                    dest_img->pxs = (tcursed_pix*)malloc(target_w * target_h * sizeof(tcursed_pix));
                    
                    layers[dest_id].is_active = 1;
                    layers[dest_id].img_data = dest_img;
                    snprintf(layers[dest_id].name, 63, "Eval_Result");
                }
                layers[dest_id].op_count++;

                size_t total_pixels = target_w * target_h;
                size_t p_idx;
                for (p_idx = 0; p_idx < total_pixels; p_idx++) {
                    RGBFloat final_color = eval_ast(root, p_idx);
                    uint16_t* dest_channels = (uint16_t*)&(dest_img->pxs[p_idx]);
                    dest_channels[0] = CLAMP16((int)final_color.r, 0, 65535);
                    dest_channels[1] = CLAMP16((int)final_color.g, 0, 65535);
                    dest_channels[2] = CLAMP16((int)final_color.b, 0, 65535);
                    dest_channels[3] = 65535; 
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