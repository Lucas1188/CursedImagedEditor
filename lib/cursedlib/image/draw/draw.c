
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
        int err2;
        err2 = err * 2;
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

void draw_triangle(size_t x0, size_t y0, size_t x1, size_t y1, size_t x2, size_t y2, tcursed_pix color, cursed_img* img){
    draw_line(x0, y0, x1, y1, color, img);
    draw_line(x1, y1, x2, y2, color, img);
    draw_line(x2, y2, x0, y0, color, img);
}

void fill_rectangle(size_t x0, size_t y0, size_t x1, size_t y1, tcursed_pix color, cursed_img* img){
    size_t x,y;
    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            if (x < img->width && y < img->height) {
                img->pxs[y * img->width + x] = color;
            }
        }
    }
}
void fill_circle(size_t x0, size_t y0, size_t radius, tcursed_pix color, cursed_img* img){
    int f = 1 - radius;
    int ddF_x = 1;
    int ddF_y = -2 * radius;
    int x = 0;
    int y = radius;
    int i;
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        /* Draw horizontal lines to fill the circle */
        for (i = x0 - x; i <= x0 + x; i++) {
            if (i >= 0 && i < img->width) {
                if (y0 + y < img->height) {
                    img->pxs[(y0 + y) * img->width + i] = color; /* Octant 1 & 2 */
                }
                if (y0 - y >= 0) {
                    img->pxs[(y0 - y) * img->width + i] = color; /* Octant 3 & 4 */
                }
            }
        }
        for (i = x0 - y; i <= x0 + y; i++) {
            if (i >= 0 && i < img->width) {
                if (y0 + x < img->height) {
                    img->pxs[(y0 + x) * img->width + i] = color; /* Octant 5 & 6 */
                }
                if (y0 - x >= 0) {
                    img->pxs[(y0 - x) * img->width + i] = color; /* Octant 7 & 8 */
                }
            }
        }
    }
}
void fill_triangle(size_t x0, size_t y0, size_t x1, size_t y1, size_t x2, size_t y2, tcursed_pix color, cursed_img* img){
    /* Sort vertices by y-coordinate ascending (y0 <= y1 <= y2) */
    if (y0 > y1) { size_t tmp; tmp = y0; y0 = y1; y1 = tmp; tmp = x0; x0 = x1; x1 = tmp; }
    if (y1 > y2) { size_t tmp; tmp = y1; y1 = y2; y2 = tmp; tmp = x1; x1 = x2; x2 = tmp; }
    if (y0 > y1) { size_t tmp; tmp = y0; y0 = y1; y1 = tmp; tmp = x0; x0 = x1; x1 = tmp; }

    int total_height = (int)(y2 - y0);
    int j, i;
    for (i = 0; i < total_height; i++) {
        int second_half = i > (int)(y1 - y0) || (int)(y1 == y0);
        int segment_height = second_half ? (int)(y2 - y1) : (int)(y1 - y0);
        float alpha = (float)i / total_height;
        float beta  = (float)(i - (second_half ? (int)(y1 - y0) : 0)) / segment_height;

        int ax = (int)x0 + (int)((x2 - x0) * alpha);
        int bx = second_half ? (int)x1 + (int)((x2 - x1) * beta) : (int)x0 + (int)((x1 - x0) * beta);

        if (ax > bx) { int tmp = ax; ax = bx; bx = tmp; }
        for (j = ax; j <= bx; j++) {
            if ((size_t)j < img->width && (size_t)(y0 + i) < img->height) {
                img->pxs[(y0 + i) * img->width + j] = color;
            }
        }
    }
}
