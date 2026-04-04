#include <stdio.h>
#include <string.h>
#include "../include/tui_state.h"

/* Instantiate the globals */
Layer layers[MAX_LAYERS] = {0};
int selected_layer_idx = -1;
char window_log[MAX_HISTORY][128] = {0};
int log_count = 0;
tcursed_pix current_color;
void (*cursed_log_callback)(const char* msg) = NULL;

void clear_screen() {
    printf("\033[2J\033[H");
}

void add_log(const char* message) {
    int i;
    if (log_count < MAX_HISTORY) {
        strncpy(window_log[log_count], message, 127);
        log_count++;
    } else {
        for (i = 1; i < MAX_HISTORY; i++) {
            strcpy(window_log[i-1], window_log[i]);
        }
        strncpy(window_log[MAX_HISTORY-1], message, 127);
    }
}

tcursed_pix make_pixel(uint16_t r, uint16_t g, uint16_t b, uint16_t a) {
    uint64_t px = 0;
    px |= ((uint64_t)r);
    px |= ((uint64_t)g << 16);
    px |= ((uint64_t)b << 32);
    px |= ((uint64_t)a << 48);
    return (tcursed_pix)px;
}