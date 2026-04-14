#include <string.h>
#include "../include/tui_help.h"
#include "../include/tui_state.h"
#include "../lib/cursedhelpers.h"

void display_help(const char* target_cmd) {
    if (target_cmd == NULL || strlen(target_cmd) == 0) {
        add_log("--- CURSED-IE HELP OVERVIEW ---");
        add_log("Type 'help <command>' for specific details.");
        add_log(" FILE  : load, ls, save");
        add_log(" LAYER : new, select, clear");
        add_log(" FX    : blur, invert, filter, color, rect");
        add_log(" MATH  : <dest> = <equation> (e.g., l3 = l1 + 0.5 * l2)");
        add_log(" SYS   : exit");
        add_log("-------------------------------");
        return;
    }

    if (strcmp(target_cmd, "load") == 0) {
        add_log("HELP: load");
        add_log("Usage: load <filename> OR load <index>");
        add_log("Loads an image from the current directory into a new layer.");
        add_log("The first image loaded defines the master Canvas size.");
    } 
    else if (strcmp(target_cmd, "ls") == 0 || strcmp(target_cmd, "list") == 0) {
        add_log("HELP: ls / list");
        add_log("Usage: ls");
        add_log("Lists all files in the current working directory.");
        add_log("Files are assigned a numerical index for easy loading.");
    }
    else if (strcmp(target_cmd, "save") == 0) {
        add_log("HELP: save");
        add_log("Usage: save [format] [filename]");
        add_log("Formats supported: png (default), bmp.");
        add_log("Example: 'save output.png' or 'save bmp result.bmp'");
    }
    else if (strcmp(target_cmd, "new") == 0) {
        add_log("HELP: new");
        add_log("Usage: new [width] [height] [name]");
        add_log("Creates a transparent layer. If Canvas size is already locked,");
        add_log("width and height are ignored (Usage becomes: new [name]).");
    }
    else if (strcmp(target_cmd, "select") == 0) {
        add_log("HELP: select");
        add_log("Usage: select <layer_name>");
        add_log("Sets the active layer for drawing and FX operations.");
    }
    else if (strcmp(target_cmd, "filter") == 0) {
        add_log("HELP: filter");
        add_log("Usage: filter <name> [args...]");
        add_log("Applies a convolution matrix or dynamic effect to the active layer.");
        add_log(" Dynamic:");
        add_log("  filter blur <radius> [sigma]  (Fast separable 4K-friendly blur)");
        add_log(" Static Kernels:");
        add_log("  edge1, edge2, sharpen, box_blur, gaussian3, gaussian5,");
        add_log("  unsharp5, emboss, sobel_x, sobel_y");
    }
    else if (strcmp(target_cmd, "color") == 0) {
        add_log("HELP: color");
        add_log("Usage: color <R> <G> <B> [A]");
        add_log("Sets the active brush color (0-255). Alpha is optional.");
        add_log("Example: 'color 255 0 0' sets the color to red.");
    }
    else if (strcmp(target_cmd, "draw") == 0) {
        add_log("HELP: draw");
        add_log("Usage: draw <shape> [coordinates...]");
        add_log("Uses the active 'color' to draw on the selected layer.");
        add_log("  draw line <x0> <y0> <x1> <y1>");
        add_log("  draw rect <x0> <y0> <x1> <y1>         | draw fillrect ...");
        add_log("  draw circle <x> <y> <radius>          | draw fillcircle ...");
        add_log("  draw triangle <x0> <y0> ... <x2> <y2> | draw filltriangle ...");
    }
    else if (strcmp(target_cmd, "clear") == 0) {
        add_log("HELP: clear");
        add_log("Safely removes layers from memory.");
        add_log("  clear         - Deletes the currently selected layer.");
        add_log("  clear <name>  - Deletes a specific layer by name.");
        add_log("  clear all     - Nukes all layers and resets canvas size.");
    }
    else {
        char msg[128];
        cursed_snprintf_fallback(msg, sizeof(msg), "Error: No help available for '%s'.", target_cmd);
        add_log(msg);
    }
}