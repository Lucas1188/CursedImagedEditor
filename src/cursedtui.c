#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>   /* For directory reading */
#include <ctype.h>    /* For isdigit */
#include "../include/parser.h"
#include "../lib/bitmap/bitmap.h"
#include "../lib/bitmap/bitmap_cursed.h"
#include "../lib/cursedlib/image/image.h"

#define MAX_LAYERS 4
#define MAX_CMD_LEN 256
#define VIEWPORT_LINES 12 /* Height of the scrollable log window */
#define MAX_HISTORY 100

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
    printf("\033[1;37;44m CLI-SHOP v0.2 | LAYERS: ");
    for (i = 0; i < MAX_LAYERS; i++) {
        if (layers[i].is_active) {
            /* Highlight the currently selected layer */
            if (i == selected_layer_idx) {
                printf("\033[1;33m[*%s* (Ops:%d)]\033[1;37;44m ", layers[i].name, layers[i].op_count);
            } else {
                printf("[%s] ", layers[i].name);
            }
        } else {
            printf("[Empty] ");
        }
    }
    printf("\033[0m\n");
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
    printf("\033[90m HELP: 'load <name>', 'select <name>', 'blur', 'invert', 'clear', 'exit'\033[0m\n");
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
/* --- EXECUTOR --- */
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

                /* 2. Check if arg1 is purely digits */
                for(i = 0; i < strlen(ast.arg1); i++) {
                    if(!isdigit(ast.arg1[i])) { 
                        is_num = 0; 
                        break; 
                    }
                }

                /* 3. Resolve the target file string */
                if (is_num) {
                    int idx = atoi(ast.arg1);
                    if (idx >= 0 && idx < file_cache_count) {
                        strncpy(target_file, file_cache[idx], 255);
                    } else {
                        add_log("Error: Invalid file index. Run 'list' to refresh.");
                        return 1;
                    }
                } else {
                    /* It's not a number, assume they typed the literal filename */
                    strncpy(target_file, ast.arg1, 255);
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
                free_bitmap(temp_bmp); /* Clean up the intermediate struct */

                if (!loaded_img) {
                    add_log("Error: Failed to convert bitmap to internal format.");
                    return 1;
                }

                /* 6. Load it into the next available UI Layer */
                for (i = 0; i < MAX_LAYERS; i++) {
                    if (!layers[i].is_active) {
                        strncpy(layers[i].name, target_file, 63);
                        layers[i].is_active = 1;
                        layers[i].op_count = 0;
                        layers[i].img_data = loaded_img; /* Bind the data to the UI state */
                        
                        selected_layer_idx = i;
                        
                        snprintf(msg_buffer, sizeof(msg_buffer), "-> Loaded '%s' (Size: %dx%d)", 
                                 layers[i].name, loaded_img->width, loaded_img->height);
                        add_log(msg_buffer);
                        return 1;
                    }
                }
                
                /* 7. If we reached here, layers are full */
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
            for (i = 0; i < MAX_LAYERS; i++) {
                if (layers[i].is_active && strcmp(layers[i].name, ast.arg1) == 0) {
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
    add_log("Welcome to CLI-SHOP. Type 'load <name>' to begin.");
    
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