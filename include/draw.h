#ifndef PHONE_DRAW_H
#define PHONE_DRAW_H

#include "types.h"

void draw_fill(Gfx *g, int x, int y, int w, int h, uint16_t c);
void draw_rect(Gfx *g, int x, int y, int w, int h, uint16_t c);
void draw_hline(Gfx *g, int x, int y, int w, uint16_t c);
void draw_vline(Gfx *g, int x, int y, int h, uint16_t c);
void draw_line(Gfx *g, int x0, int y0, int x1, int y1, uint16_t c);
void draw_round_rect(Gfx *g, int x, int y, int w, int h, int r, uint16_t c, int fill);
void draw_elevated_rect(Gfx *g, int x, int y, int w, int h, int r, uint16_t fill, uint16_t border);
void draw_pixel(Gfx *g, int x, int y, uint16_t c);
void draw_pixel_aa(Gfx *g, int x, int y, uint16_t c, uint8_t cover);

#endif
