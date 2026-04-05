#ifndef TUI_STATE_H
#define TUI_STATE_H

#include <stdint.h>
#include "../lib/cursedlib/image/image.h"

#define MAX_LAYERS 4
#define MAX_HISTORY 100
#define VIEWPORT_LINES 12

#define CLAMP(x, low, high)    (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define CLAMP16(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

typedef struct {
    char name[64];
    int is_active;
    int op_count;
    cursed_img* img_data;
} Layer;

/* Global state variables shared across the app */
extern Layer layers[MAX_LAYERS];
extern int selected_layer_idx;
extern char window_log[MAX_HISTORY][128];
extern int log_count;
extern tcursed_pix current_color;
extern void (*cursed_log_callback)(const char* msg);

extern int canvas_width;
extern int canvas_height;
/* Core helpers */
void clear_screen();
void add_log(const char* message);
tcursed_pix make_pixel(uint16_t r, uint16_t g, uint16_t b, uint16_t a);
int get_layer_idx_by_name(const char* name);
#endif