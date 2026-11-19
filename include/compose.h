#ifndef VILAOS_COMPOSE_H
#define VILAOS_COMPOSE_H

#include "types.h"
#include "hwinfo.h"
#include "pool.h"

#define COMPOSE_MAX_CMDS 512
#define COMPOSE_MAX_CLIP 8

typedef enum {
    CCMD_CLEAR = 0,
    CCMD_FILL,
    CCMD_ROUND_RECT,
    CCMD_GRADIENT,
    CCMD_LINE_AA,
    CCMD_TEXT
} ComposeCmdType;

typedef struct {
    ComposeCmdType type;
    int x, y, w, h;
    int x1, y1;
    int radius;
    int fill;
    int vertical;
    uint32_t c0;
    uint32_t c1;
    uint8_t alpha;
} ComposeCmd;

struct Composer {
    uint32_t *px;
    size_t pixels;
    int w, h, stride;
    ComposeCmd cmds[COMPOSE_MAX_CMDS];
    int cmd_count;
    Rect clip[COMPOSE_MAX_CLIP];
    int clip_depth;
    DirtyRect dirty;
    GfxPool pool;
    HwProfile hw;
    int fade_alpha;
};

uint32_t compose_rgb565_to_argb(uint16_t c);
uint16_t compose_argb_to_rgb565(uint32_t c);
uint32_t compose_blend(uint32_t dst, uint32_t src);

int compose_init(Composer *c, int w, int h, const HwProfile *hw);
void compose_shutdown(Composer *c);
int compose_resize(Composer *c, int w, int h);
void compose_begin(Composer *c);
void compose_set_clip(Composer *c, int x, int y, int w, int h);
void compose_clear(Composer *c, uint32_t argb);
void compose_fill(Composer *c, int x, int y, int w, int h, uint32_t argb);
void compose_round_rect(Composer *c, int x, int y, int w, int h, int r, uint32_t argb, int fill);
void compose_gradient(Composer *c, int x, int y, int w, int h, uint32_t c0, uint32_t c1, int vertical);
void compose_line_aa(Composer *c, int x0, int y0, int x1, int y1, uint32_t argb);
void compose_dirty(Composer *c, int x, int y, int w, int h);
void compose_dirty_all(Composer *c);
void compose_flush(Composer *c, uint16_t *dst565, int dst_w, int dst_h);
void compose_mark_fade(Composer *c, int alpha);

#endif
