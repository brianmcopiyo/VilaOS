#ifndef PHONE_FB_H
#define PHONE_FB_H

#include "types.h"

int gfx_init(Gfx *g, int desktop);
void gfx_shutdown(Gfx *g);
int gfx_set_size(Gfx *g, int w, int h);
void gfx_dirty(Gfx *g, int x, int y, int w, int h);
void gfx_dirty_all(Gfx *g);
void gfx_present(Gfx *g);
void gfx_clear(Gfx *g, uint16_t color);
uint16_t *gfx_pixels(Gfx *g);
int gfx_poll_desktop(Gfx *g);
const char *gfx_format_label(Gfx *g, char *buf, size_t buf_sz);

#endif
