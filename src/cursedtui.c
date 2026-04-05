#include <stdio.h>
#include <string.h>

#include "../include/cursedtui.h"
#include "../include/tui_state.h"
#include "../include/tui_exec.h"
#include "../include/parser.h"

#define MAX_CMD_LEN 256

void draw_ui() {
    int i;
    clear_screen();
    
    printf("\033[1;37;44m CURSED IMG EDTR | CANVAS: %dx%d | LAYERS: ", canvas_width, canvas_height);
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
    
    uint16_t* c = (uint16_t*)&current_color;
    int r8 = c[0] / 257; 
    int g8 = c[1] / 257; 
    int b8 = c[2] / 257;
    
    printf(" | COLOR: \033[48;2;%d;%d;%dm    \033[0m\n", r8, g8, b8);
    printf("--------------------------------------------------------------------------------\n");
    
    int start_idx = (log_count > VIEWPORT_LINES) ? (log_count - VIEWPORT_LINES) : 0;
    
    for (i = 0; i < VIEWPORT_LINES; i++) {
        if (start_idx + i < log_count) {
            printf(" %s\n", window_log[start_idx + i]);
        } else {
            printf("\n");
        }
    }
    
    printf("--------------------------------------------------------------------------------\n");
    printf("\033[90m CMD : load <n/name>, ls, save [fmt], new [w] [h], select <n>, clear, exit \033[0m\n");
    printf("\033[90m FX  : blur, invert, rect | MATH: l<id> = [w]*l<id> +/- ... (e.g. l2=0.5*l1+l0) \033[0m\n");
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