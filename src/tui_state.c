#include <stdio.h>
#include <string.h>
#include "../include/tui_state.h"
#include "../lib/cursedhelpers.h"
/* Instantiate the globals */
Layer layers[MAX_LAYERS] = {0};
int selected_layer_idx = -1;
char window_log[MAX_HISTORY][128] = {0};
int log_count = 0;
tcursed_pix current_color;
#ifndef BUILD_PACKER
void (*cursed_log_callback)(const char* msg) = NULL;
#endif
/* ... below int selected_layer_idx = -1; ... */
int canvas_width = 0;
int canvas_height = 0;

void clear_screen() {
    printf("\033[2J\033[H");
}

void add_log(const char* message) {
    int i;
    if (log_count < MAX_HISTORY) {
        cursed_strncpy(window_log[log_count], message, 127);
        log_count++;
    } else {
        for (i = 1; i < MAX_HISTORY; i++) {
            strcpy(window_log[i-1], window_log[i]);
        }
        cursed_strncpy(window_log[MAX_HISTORY-1], message, 127);
    }
}

tcursed_pix make_pixel(uint16_t r, uint16_t g, uint16_t b, uint16_t a) {
    /* Rely on the macro to guarantee R-G-B-A order in memory */
    return (tcursed_pix)PACK_RGBA64(r, g, b, a);
}

int get_layer_idx_by_name(const char* name) {
    int i;
    for (i = 0; i < MAX_LAYERS; i++) {
        if (layers[i].is_active && strcmp(layers[i].name, name) == 0) return i;
    }
    return -1; /* Layer not found */
}