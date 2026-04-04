#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../../cursedhelpers.h"
#include "../image.h"

void draw_circle(size_t x0, size_t y0, size_t radius, tcursed_pix color, cursed_img* img);
void draw_line(size_t x0, size_t y0, size_t x1, size_t y1, tcursed_pix color, cursed_img* img);
void draw_rectangle(size_t x0, size_t y0, size_t x1, size_t y1, tcursed_pix color, cursed_img* img);
void draw_triangle(size_t x0, size_t y0, size_t x1, size_t y1, size_t x2, size_t y2, tcursed_pix color, cursed_img* img);

void fill_rectangle(size_t x0, size_t y0, size_t x1, size_t y1, tcursed_pix color, cursed_img* img);
void fill_circle(size_t x0, size_t y0, size_t radius, tcursed_pix color, cursed_img* img);
void fill_triangle(size_t x0, size_t y0, size_t x1, size_t y1, size_t x2, size_t y2, tcursed_pix color, cursed_img* img);