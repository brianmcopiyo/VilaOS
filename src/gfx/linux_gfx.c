#include "linux_gfx.h"
#include "fb.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(VILAOS_HAVE_LINUX_GFX) && !defined(PHONE_DESKTOP) && !defined(_WIN32)

#include <cairo.h>
#include <pixman.h>

static void clip_union(pixman_region32_t *reg, int x, int y, int w, int h)
{
    if (!reg || w <= 0 || h <= 0)
        return;
    pixman_region32_union_rect(reg, reg, x, y, (unsigned)w, (unsigned)h);
}

typedef struct {
    cairo_surface_t *surf;
    cairo_t *cr;
    uint32_t *argb;
    int w;
    int h;
    int stride;
} LinuxGfxState;

static uint32_t rgb565_to_argb(uint16_t c)
{
    uint32_t r = (c >> 11) & 0x1F;
    uint32_t g = (c >> 5) & 0x3F;
    uint32_t b = c & 0x1F;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static uint16_t argb_to_rgb565(uint32_t c)
{
    uint32_t r = (c >> 16) & 0xFF;
    uint32_t g = (c >> 8) & 0xFF;
    uint32_t b = c & 0xFF;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void set_source(cairo_t *cr, uint16_t c)
{
    uint32_t a = rgb565_to_argb(c);
    cairo_set_source_rgba(cr,
                          ((a >> 16) & 0xFF) / 255.0,
                          ((a >> 8) & 0xFF) / 255.0,
                          (a & 0xFF) / 255.0,
                          1.0);
}

static LinuxGfxState *state_of(Gfx *g)
{
    return g ? (LinuxGfxState *)g->linux_gfx : NULL;
}

static void flush_to_back(Gfx *g, int x, int y, int w, int h)
{
    LinuxGfxState *st = state_of(g);
    if (!st || !g || !g->back || !st->argb || w <= 0 || h <= 0)
        return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > g->w)
        w = g->w - x;
    if (y + h > g->h)
        h = g->h - y;
    if (w <= 0 || h <= 0)
        return;
    for (int row = y; row < y + h; row++) {
        uint32_t *src = st->argb + row * st->stride + x;
        uint16_t *dst = g->back + row * g->w + x;
        for (int col = 0; col < w; col++)
            dst[col] = argb_to_rgb565(src[col]);
    }
    gfx_dirty(g, x, y, w, h);
}

int linux_gfx_init(Gfx *g)
{
    if (!g)
        return -1;
    linux_gfx_shutdown(g);
    LinuxGfxState *st = (LinuxGfxState *)calloc(1, sizeof(*st));
    if (!st)
        return -1;
    g->linux_gfx = st;
    if (g->w > 0 && g->h > 0)
        return linux_gfx_resize(g, g->w, g->h);
    return 0;
}

void linux_gfx_shutdown(Gfx *g)
{
    if (!g || !g->linux_gfx)
        return;
    LinuxGfxState *st = state_of(g);
    if (st->cr)
        cairo_destroy(st->cr);
    if (st->surf)
        cairo_surface_destroy(st->surf);
    free(st->argb);
    free(st);
    g->linux_gfx = NULL;
}

int linux_gfx_resize(Gfx *g, int w, int h)
{
    if (!g || w < 1 || h < 1)
        return -1;
    if (!g->linux_gfx && linux_gfx_init(g) != 0)
        return -1;
    LinuxGfxState *st = state_of(g);
    if (st->cr) {
        cairo_destroy(st->cr);
        st->cr = NULL;
    }
    if (st->surf) {
        cairo_surface_destroy(st->surf);
        st->surf = NULL;
    }
    free(st->argb);
    st->argb = NULL;
    st->w = w;
    st->h = h;
    st->stride = w;
    st->argb = (uint32_t *)calloc((size_t)w * (size_t)h, sizeof(uint32_t));
    if (!st->argb)
        return -1;
    st->surf = cairo_image_surface_create_for_data(
        (unsigned char *)st->argb, CAIRO_FORMAT_ARGB32, w, h, w * 4);
    if (cairo_surface_status(st->surf) != CAIRO_STATUS_SUCCESS)
        return -1;
    st->cr = cairo_create(st->surf);
    if (cairo_status(st->cr) != CAIRO_STATUS_SUCCESS)
        return -1;
    cairo_set_antialias(st->cr, CAIRO_ANTIALIAS_BEST);
    g->compose_px = st->argb;
    g->compose_pixels = (size_t)w * (size_t)h;
    return 0;
}

int linux_gfx_ready(const Gfx *g)
{
    const LinuxGfxState *st = g ? (const LinuxGfxState *)g->linux_gfx : NULL;
    return st && st->cr && st->surf && st->argb;
}

void linux_gfx_sync_dirty(Gfx *g)
{
    if (!g || !g->dirty.valid)
        return;
    flush_to_back(g, g->dirty.x0, g->dirty.y0, g->dirty.x1 - g->dirty.x0, g->dirty.y1 - g->dirty.y0);
}

void linux_gfx_fill(Gfx *g, int x, int y, int w, int h, uint16_t c)
{
    LinuxGfxState *st = state_of(g);
    if (!linux_gfx_ready(g) || w <= 0 || h <= 0)
        return;
    pixman_region32_t clip;
    pixman_region32_init(&clip);
    clip_union(&clip, x, y, w, h);
    set_source(st->cr, c);
    cairo_rectangle(st->cr, x, y, w, h);
    cairo_fill(st->cr);
    flush_to_back(g, x, y, w, h);
    pixman_region32_fini(&clip);
}

void linux_gfx_rect(Gfx *g, int x, int y, int w, int h, uint16_t c)
{
    LinuxGfxState *st = state_of(g);
    if (!linux_gfx_ready(g) || w <= 0 || h <= 0)
        return;
    set_source(st->cr, c);
    cairo_set_line_width(st->cr, 1.0);
    cairo_rectangle(st->cr, x + 0.5, y + 0.5, w - 1.0, h - 1.0);
    cairo_stroke(st->cr);
    flush_to_back(g, x - 1, y - 1, w + 2, h + 2);
}

void linux_gfx_line(Gfx *g, int x0, int y0, int x1, int y1, uint16_t c)
{
    LinuxGfxState *st = state_of(g);
    if (!linux_gfx_ready(g))
        return;
    set_source(st->cr, c);
    cairo_set_line_width(st->cr, 1.0);
    cairo_set_line_cap(st->cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(st->cr, x0 + 0.5, y0 + 0.5);
    cairo_line_to(st->cr, x1 + 0.5, y1 + 0.5);
    cairo_stroke(st->cr);
    int minx = x0 < x1 ? x0 : x1;
    int miny = y0 < y1 ? y0 : y1;
    int maxx = x0 > x1 ? x0 : x1;
    int maxy = y0 > y1 ? y0 : y1;
    flush_to_back(g, minx - 1, miny - 1, maxx - minx + 3, maxy - miny + 3);
}

void linux_gfx_round_rect(Gfx *g, int x, int y, int w, int h, int r, uint16_t c, int fill)
{
    LinuxGfxState *st = state_of(g);
    if (!linux_gfx_ready(g) || w <= 0 || h <= 0)
        return;
    if (r < 0)
        r = 0;
    if (r * 2 > w)
        r = w / 2;
    if (r * 2 > h)
        r = h / 2;
    double rr = (double)r;
    double x0 = x;
    double y0 = y;
    double x1 = x + w;
    double y1 = y + h;
    set_source(st->cr, c);
    cairo_new_sub_path(st->cr);
    cairo_arc(st->cr, x1 - rr, y0 + rr, rr, -3.14159265358979323846 / 2.0, 0);
    cairo_arc(st->cr, x1 - rr, y1 - rr, rr, 0, 3.14159265358979323846 / 2.0);
    cairo_arc(st->cr, x0 + rr, y1 - rr, rr, 3.14159265358979323846 / 2.0, 3.14159265358979323846);
    cairo_arc(st->cr, x0 + rr, y0 + rr, rr, 3.14159265358979323846, 3.0 * 3.14159265358979323846 / 2.0);
    cairo_close_path(st->cr);
    if (fill)
        cairo_fill(st->cr);
    else {
        cairo_set_line_width(st->cr, 1.0);
        cairo_stroke(st->cr);
    }
    flush_to_back(g, x - 1, y - 1, w + 2, h + 2);
}

void linux_gfx_pixel(Gfx *g, int x, int y, uint16_t c)
{
    LinuxGfxState *st = state_of(g);
    if (!linux_gfx_ready(g) || x < 0 || y < 0 || x >= g->w || y >= g->h)
        return;
    st->argb[y * st->stride + x] = rgb565_to_argb(c);
    g->back[y * g->w + x] = c;
    gfx_dirty(g, x, y, 1, 1);
}

void linux_gfx_pixel_aa(Gfx *g, int x, int y, uint16_t c, uint8_t cover)
{
    LinuxGfxState *st = state_of(g);
    if (!linux_gfx_ready(g) || cover == 0 || x < 0 || y < 0 || x >= g->w || y >= g->h)
        return;
    if (cover >= 255) {
        linux_gfx_pixel(g, x, y, c);
        return;
    }
    uint32_t src = rgb565_to_argb(c);
    uint32_t dst = st->argb[y * st->stride + x];
    uint32_t a = cover;
    uint32_t inv = 255 - a;
    uint32_t r = ((((src >> 16) & 0xFF) * a) + (((dst >> 16) & 0xFF) * inv)) / 255;
    uint32_t gg = ((((src >> 8) & 0xFF) * a) + (((dst >> 8) & 0xFF) * inv)) / 255;
    uint32_t b = (((src & 0xFF) * a) + ((dst & 0xFF) * inv)) / 255;
    uint32_t out = 0xFF000000u | (r << 16) | (gg << 8) | b;
    st->argb[y * st->stride + x] = out;
    g->back[y * g->w + x] = argb_to_rgb565(out);
    gfx_dirty(g, x, y, 1, 1);
}

#else

int linux_gfx_init(Gfx *g)
{
    (void)g;
    return -1;
}
void linux_gfx_shutdown(Gfx *g)
{
    (void)g;
}
int linux_gfx_resize(Gfx *g, int w, int h)
{
    (void)g;
    (void)w;
    (void)h;
    return -1;
}
int linux_gfx_ready(const Gfx *g)
{
    (void)g;
    return 0;
}
void linux_gfx_sync_dirty(Gfx *g)
{
    (void)g;
}
void linux_gfx_fill(Gfx *g, int x, int y, int w, int h, uint16_t c)
{
    (void)g;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)c;
}
void linux_gfx_rect(Gfx *g, int x, int y, int w, int h, uint16_t c)
{
    (void)g;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)c;
}
void linux_gfx_line(Gfx *g, int x0, int y0, int x1, int y1, uint16_t c)
{
    (void)g;
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)c;
}
void linux_gfx_round_rect(Gfx *g, int x, int y, int w, int h, int r, uint16_t c, int fill)
{
    (void)g;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)r;
    (void)c;
    (void)fill;
}
void linux_gfx_pixel(Gfx *g, int x, int y, uint16_t c)
{
    (void)g;
    (void)x;
    (void)y;
    (void)c;
}
void linux_gfx_pixel_aa(Gfx *g, int x, int y, uint16_t c, uint8_t cover)
{
    (void)g;
    (void)x;
    (void)y;
    (void)c;
    (void)cover;
}

#endif
