#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
char *getcwd(char *buf, size_t size);
#endif

#include "../include/cursedtui.h"
#include "../include/tui_state.h"
#include "../include/tui_exec.h"
#include "../include/parser.h"
#include "../lib/cursedlib/image/image.h"
#include "../lib/bitmap/bitmap_cursed.h"
#include "../lib/png/png.h"
#include "../lib/bitmap/bitmap.h"
#include "../include/cursed_viewer.h"

#define MAX_CMD_LEN 376

/* Creates a fast, low-res copy of an image for the real-time monitor */
static cursed_img* cursed_create_proxy(cursed_img* src, int scale) {
    cursed_img* proxy;
    size_t pw, ph, px, py;
    
    if (!src || scale <= 1) return NULL;

    pw = src->width / scale;
    ph = src->height / scale;
    
    proxy = (cursed_img*)malloc(sizeof(cursed_img));
    if (!proxy) return NULL;
    
    proxy->width = pw;
    proxy->height = ph;
    proxy->pxs = (uint64_t*)malloc(pw * ph * sizeof(uint64_t));
    
    if (!proxy->pxs) {
        free(proxy);
        return NULL;
    }

    /* Lightning-fast nearest-neighbor downsampling */
    for (py = 0; py < ph; py++) {
        for (px = 0; px < pw; px++) {
            size_t src_y = py * scale;
            size_t src_x = px * scale;
            proxy->pxs[py * pw + px] = src->pxs[src_y * src->width + src_x];
        }
    }

    return proxy;
}

void log_clickable_link(const char* full_path, const char* label) {
    char link_buffer[2048];
    
    /* \033]8;;      -> Start Hyperlink
       %s            -> The URL
       \033\\        -> ST (String Terminator) to end URL part
       %s            -> The visible text
       \033]8;;      -> Start closing sequence
       \033\\        -> ST to close the hyperlink entirely
    */
    sprintf(link_buffer, "\033]8;;%s\033\\", full_path);
    
    add_log(link_buffer);
    sprintf(link_buffer, "%s\033]8;;\033\\", label); /* Reset hyperlink */
    /* \033[0m resets colors/underlines; \033]8;;\033\\ resets links */
    add_log(link_buffer);
    add_log("\033[0m\033]8;;\033\\\n");
}

void draw_ui() {
    int i;
    int active_count = 0;
    clear_screen();
    
    /* 1. Calculate Terminal Color */
    uint16_t* c = (uint16_t*)&current_color;
    int r8 = c[3] / 257; 
    int g8 = c[2] / 257; 
    int b8 = c[1] / 257;
    int a8 = c[0] / 257;
    
    /* 2. Top Header Line: App Info, Canvas, Color Block, and Visual Alpha Block */
    printf("\033[1;37;44m CURSED IMG EDTR | CANVAS: %dx%d | COLOR: \033[48;2;%d;%d;%dm    \033[0m\033[1;37;44m | ALPHA: %3d \033[48;2;%d;%d;%dm    \033[0m\n", 
           canvas_width, canvas_height, r8, g8, b8, a8, a8, a8, a8);
           
    /* 3. Second Header Line: The Layer Stack */
    printf("\033[1;37;44m LAYERS: ");
    
    for (i = 0; i < MAX_LAYERS; i++) {
        if (layers[i].is_active) {
            active_count++;
            if (i == selected_layer_idx) {
                printf("\033[1;33m[*%s* (Ops:%d)]\033[1;37;44m ", layers[i].name, layers[i].op_count);
            } else {
                printf("[%s] ", layers[i].name);
            }
        }
    }
    
    /* Only print 'Empty' if there are literally 0 layers loaded */
    if (active_count == 0) {
        printf("[Empty Canvas] ");
    }
    
    /* Safely close the blue background line */
    printf("\033[0m\n");
    
    printf("--------------------------------------------------------------------------------\n");
    
    /* 4. Scrollable Log */
    int start_idx = (log_count > VIEWPORT_LINES) ? (log_count - VIEWPORT_LINES) : 0;
    
    for (i = 0; i < VIEWPORT_LINES; i++) {
        if (start_idx + i < log_count) {
            printf(" %s\n", window_log[start_idx + i]);
        } else {
            printf("\n");
        }
    }
    
    /* 5. Footer Menu */
    printf("--------------------------------------------------------------------------------\n");
    printf("\033[90m CMD : load <n/name>, ls, save [fmt], new [w] [h], select <n>, clear, exit \033[0m\n");
    printf("\033[90m FX  : blur, invert, draw, filter <name> | MATH: l<id> = [w]*l<id> +/- ... \033[0m\n");
}

void generate_placeholder(){

    uint32_t black_pixel = 0xFF000000; /* AARRGGBB */
    ihdr_chunk ihdr = IHDR_TRUECOLOR8_A8(1, 1);
    png_s* dummy_png = create_png(&ihdr, (uint8_t*)&black_pixel, 4);
    if (dummy_png) {
        write_png("temp_export.png", dummy_png);
        free_png(dummy_png);
    }
    
}
#ifndef BUILD_PACKER
int interactive_mode() {
    char input_buffer[MAX_CMD_LEN];
    cursed_log_callback = add_log; 
    current_color = make_pixel(65535, 0, 0, 65535);
    add_log("Welcome to CURSED-IE.");
    
    /* 1. Generate the HTML file */
    generate_cursed_viewer();
    remove("temp_export.png");
    /* 2. Generate a dummy log file to prevent browser 404s */
    FILE* log_f = fopen("cursed_log.js", "w");
    if (log_f) {
        fprintf(log_f, "if(window.updateTerm) window.updateTerm(`> ENGINE BOOTED.\\n> READY FOR COMMANDS.\\n`);\n");
        fclose(log_f);
    }
    
    char cwd[1024];
    char url_buffer[2048];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        sprintf(url_buffer, "file://%s/cursed_viewer.html", cwd);
    } else {
        sprintf(url_buffer, "file://cursed_viewer.html");
    }
    
    log_clickable_link(url_buffer, "-> Monitor active. Click to Open 'cursed_viewer.html' in your browser.");
    
   
    while(1) {
        draw_ui();
        
        printf("\n> ");
        if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
            break; 
        }
        
        input_buffer[strcspn(input_buffer, "\n")] = 0; 
        if (strlen(input_buffer) > 0) {
            char echo[MAX_CMD_LEN];
            sprintf(echo, "> %.372s", input_buffer);
            add_log(echo);
        }
        
        CommandAST current_cmd = parse_command(input_buffer);
        int should_continue = execute_command(current_cmd);
        /* --- AUTO-UPDATE MONITOR --- */
        if (current_cmd.type == CMD_LOAD   || current_cmd.type == CMD_NEW    || current_cmd.type == CMD_SELECT || 
            current_cmd.type == CMD_CLEAR  || current_cmd.type == CMD_DRAW   || current_cmd.type == CMD_FILTER || 
            current_cmd.type == CMD_EVAL) {
            if (selected_layer_idx != -1 && layers[selected_layer_idx].img_data != NULL) {
                cursed_img* active = layers[selected_layer_idx].img_data;
                cursed_img* proxy = NULL;
                bitmap* out_bmp = NULL;
                ihdr_chunk ihdr;
                png_s* png;
                
                int scale = 1;

                /* Auto-detect massive canvases and enable proxy mode */
                if (active->width >= 3840) scale = 4;      /* 4K -> 960x540 */
                else if (active->width >= 1920) scale = 2; /* 1080p -> 960x540 */

                /* 1. Generate Proxy if needed */
                if (scale > 1) {
                    proxy = cursed_create_proxy(active, scale);
                    out_bmp = cursed_to_bitmap(proxy);
                    free(proxy->pxs);
                    free(proxy);
                } else {
                    out_bmp = cursed_to_bitmap(active);
                }

                /* 2. Encode to PNG (Now 16x faster for 4K files!) */
                if (out_bmp) {
                    ihdr = IHDR_TRUECOLOR8_A8(out_bmp->width, out_bmp->height);
                    png = create_png(&ihdr, out_bmp->pixels, 4);
                    
                    if (png) {
                        write_png("temp_export.png", png);
                        free_png(png);
                    }
                    free_bitmap(out_bmp);
                }
            }

            /* Generate a dynamic JS payload to bypass browser security */
            FILE* f = fopen("cursed_log.js", "w");
            if (f) {
                int i;
                /* We wrap the logs in a JS function call and an ES6 template literal (backticks) */
                fprintf(f, "if(window.updateTerm) window.updateTerm(`> ENGINE LOG SYNCED\\n\\n");
                for (i = 0; i < log_count; i++) {
                    fprintf(f, "%s\n", window_log[i]);
                }
                fprintf(f, "`);\n");
                fclose(f);
            }
        }
        
        if (!should_continue) {
            break;
        }
    }
    
    clear_screen();
    return 1;
}
#endif