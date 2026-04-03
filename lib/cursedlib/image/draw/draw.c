
#include "draw.h"

void draw_circle(size_t x0, size_t y0, size_t radius, tcursed_pix color, cursed_img* img){
    int f = 1 - radius;
    int ddF_x = 1;
    int ddF_y = -2 * radius;
    int x = 0;
    int y = radius;

    if (y0 + radius < img->height) {
        img->pxs[y0 + radius * img->width + x0] = color; /* 12 o'clock */
    }
    if (y0 - radius >= 0) {
        img->pxs[(y0 - radius) * img->width + x0] = color; /* 6 o'clock */
    }
    if (x0 + radius < img->width) {
        img->pxs[y0 * img->width + x0 + radius] = color; /* 3 o'clock */
    }
    if (x0 - radius >= 0) {
        img->pxs[y0 * img->width + x0 - radius] = color; /* 9 o'clock */
    }

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (y0 + y < img->height && x0 + x < img->width) {
            img->pxs[(y0 + y) * img->width + x0 + x] = color; /* Octant 1 */
        }
        if (y0 + y < img->height && x0 - x >= 0) {
            img->pxs[(y0 + y) * img->width + x0 - x] = color; /* Octant 2 */
        }
        if (y0 - y >= 0 && x0 + x < img->width) {
            img->pxs[(y0 - y) * img->width + x0 + x] = color; /* Octant 4 */
        }
        if (y0 - y >= 0 && x0 - x >= 0) {
            img->pxs[(y0 - y) * img->width + x0 - x] = color; /* Octant 3 */
        }
        if (y0 + x < img->height && x0 + y < img->width) {
            img->pxs[(y0 + x) * img->width + x0 + y] = color; /* Octant 8 */
        }
        if (y0 + x < img->height && x0 - y >= 0) {
            img->pxs[(y0 + x) * img->width + x0 - y] = color; /* Octant 7 */
        }
        if (y0 - x >= 0 && x0 + y < img->width) {
            img->pxs[(y0 - x) * img->width + x0 + y] = color; /* Octant 5 */
        }
        if (y0 - x >= 0 && x0 - y >= 0) {
            img->pxs[(y0 - x) * img->width + x0 - y] = color; /* Octant 6 */
        }
    }
}
void draw_line(size_t x0, size_t y0, size_t x1, size_t y1, tcursed_pix color, cursed_img* img){
    int dx = abs((int)x1 - (int)x0);
    int dy = abs((int)y1 - (int)y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    /*No AA*/
    while (1) {
        if (x0 < img->width && y0 < img->height) {
            img->pxs[y0 * img->width + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        int err2 = err * 2;
        if (err2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (err2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}
void draw_rectangle(size_t x0, size_t y0, size_t x1, size_t y1, tcursed_pix color, cursed_img* img){
    draw_line(x0, y0, x1, y0, color, img); /* Top edge */
    draw_line(x0, y1, x1, y1, color, img); /* Bottom edge */
    draw_line(x0, y0, x0, y1, color, img); /* Left edge */
    draw_line(x1, y0, x1, y1, color, img); /* Right edge */
}
