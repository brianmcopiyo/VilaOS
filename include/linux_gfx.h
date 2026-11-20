#ifndef VILAOS_LINUX_GFX_H
#define VILAOS_LINUX_GFX_H

#include "types.h"

int linux_gfx_init(Gfx *g);
void linux_gfx_shutdown(Gfx *g);
int linux_gfx_resize(Gfx *g, int w, int h);
int linux_gfx_ready(const Gfx *g);
void linux_gfx_sync_dirty(Gfx *g);

void linux_gfx_fill(Gfx *g, int x, int y, int w, int h, uint16_t c);
void linux_gfx_rect(Gfx *g, int x, int y, int w, int h, uint16_t c);
void linux_gfx_line(Gfx *g, int x0, int y0, int x1, int y1, uint16_t c);
void linux_gfx_round_rect(Gfx *g, int x, int y, int w, int h, int r, uint16_t c, int fill);
void linux_gfx_pixel(Gfx *g, int x, int y, uint16_t c);
void linux_gfx_pixel_aa(Gfx *g, int x, int y, uint16_t c, uint8_t cover);

int linux_font_init(void);
void linux_font_shutdown(void);
int linux_font_ready(void);
int linux_font_text_w(const char *path, int size_px, int weight, const char *text);
int linux_font_char_h(int size_px);
void linux_font_draw(Gfx *g, const char *path, int size_px, int weight, int x, int y,
                     const char *text, uint16_t fg, uint16_t bg, int opaque, int max_w);

int linux_png_load(const char *path, uint8_t **rgba, int *w, int *h);
void linux_png_free(uint8_t *rgba);

int linux_drm_open(Gfx *g);
void linux_drm_close(Gfx *g);
int linux_drm_present(Gfx *g);

#endif
