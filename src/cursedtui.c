#include <stdio.h>
#include <string.h>

#include "../include/cursedtui.h"
#include "../include/tui_state.h"
#include "../include/tui_exec.h"
#include "../include/parser.h"

#define MAX_CMD_LEN 256

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

int interactive_mode() {
    char input_buffer[MAX_CMD_LEN];
    cursed_log_callback = add_log; 
    current_color = make_pixel(65535, 0, 0, 65535);
    add_log("Welcome to CURSED-IE.");
    
    while(1) {
        draw_ui();
        
        printf("\n> ");
        if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
            break; 
        }
        
        input_buffer[strcspn(input_buffer, "\n")] = 0; 
        
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