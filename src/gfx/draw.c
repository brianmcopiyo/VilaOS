#include "draw.h"
#include "fb.h"
#include "compose.h"
#include "util.h"

void draw_pixel(Gfx *g, int x, int y, uint16_t c)
{
    if (!g || !g->back || x < 0 || y < 0 || x >= g->w || y >= g->h)
        return;
    g->back[y * g->w + x] = c;
    if (g->composer && g->composer->px && x < g->composer->w && y < g->composer->h)
        g->composer->px[y * g->composer->stride + x] = compose_rgb565_to_argb(c);
    gfx_dirty(g, x, y, 1, 1);
}

void draw_fill(Gfx *g, int x, int y, int w, int h, uint16_t c)
{
    if (!g || !g->back || w <= 0 || h <= 0)
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
    uint32_t argb = compose_rgb565_to_argb(c);
    for (int row = y; row < y + h; row++) {
        uint16_t *p = &g->back[row * g->w + x];
        for (int col = 0; col < w; col++)
            p[col] = c;
        if (g->composer && g->composer->px && row < g->composer->h) {
            int x1 = x + w;
            if (x1 > g->composer->w)
                x1 = g->composer->w;
            uint32_t *cp = &g->composer->px[row * g->composer->stride + x];
            for (int col = x; col < x1; col++)
                *cp++ = argb;
        }
    }
    gfx_dirty(g, x, y, w, h);
}

void draw_hline(Gfx *g, int x, int y, int w, uint16_t c)
{
    draw_fill(g, x, y, w, 1, c);
}

void draw_vline(Gfx *g, int x, int y, int h, uint16_t c)
{
    draw_fill(g, x, y, 1, h, c);
}

void draw_rect(Gfx *g, int x, int y, int w, int h, uint16_t c)
{
    if (w <= 0 || h <= 0)
        return;
    draw_hline(g, x, y, w, c);
    draw_hline(g, x, y + h - 1, w, c);
    draw_vline(g, x, y, h, c);
    draw_vline(g, x + w - 1, y, h, c);
}

void draw_line(Gfx *g, int x0, int y0, int x1, int y1, uint16_t c)
{
    if (!g)
        return;
    if (g->composer) {
        compose_line_aa(g->composer, x0, y0, x1, y1, compose_rgb565_to_argb(c));
        compose_flush(g->composer, g->back, g->w, g->h);
        int minx = x0 < x1 ? x0 : x1;
        int miny = y0 < y1 ? y0 : y1;
        int maxx = x0 > x1 ? x0 : x1;
        int maxy = y0 > y1 ? y0 : y1;
        gfx_dirty(g, minx - 1, miny - 1, maxx - minx + 3, maxy - miny + 3);
        return;
    }
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int minx = x0 < x1 ? x0 : x1;
    int miny = y0 < y1 ? y0 : y1;
    int maxx = x0 > x1 ? x0 : x1;
    int maxy = y0 > y1 ? y0 : y1;
    for (;;) {
        draw_pixel(g, x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
    gfx_dirty(g, minx, miny, maxx - minx + 1, maxy - miny + 1);
}

void draw_round_rect(Gfx *g, int x, int y, int w, int h, int r, uint16_t c, int fill)
{
    if (w <= 0 || h <= 0)
        return;
    if (g && g->composer) {
        compose_round_rect(g->composer, x, y, w, h, r, compose_rgb565_to_argb(c), fill);
        compose_flush(g->composer, g->back, g->w, g->h);
        gfx_dirty(g, x, y, w, h);
        return;
    }
    if (r < 0)
        r = 0;
    if (r * 2 > w)
        r = w / 2;
    if (r * 2 > h)
        r = h / 2;

    if (fill) {
        draw_fill(g, x + r, y, w - 2 * r, h, c);
        draw_fill(g, x, y + r, r, h - 2 * r, c);
        draw_fill(g, x + w - r, y + r, r, h - 2 * r, c);
        for (int j = 0; j < r; j++) {
            for (int i = 0; i < r; i++) {
                int dx = r - 1 - i;
                int dy = r - 1 - j;
                if (dx * dx + dy * dy <= r * r) {
                    draw_pixel(g, x + i, y + j, c);
                    draw_pixel(g, x + w - 1 - i, y + j, c);
                    draw_pixel(g, x + i, y + h - 1 - j, c);
                    draw_pixel(g, x + w - 1 - i, y + h - 1 - j, c);
                }
            }
        }
    } else {
        draw_hline(g, x + r, y, w - 2 * r, c);
        draw_hline(g, x + r, y + h - 1, w - 2 * r, c);
        draw_vline(g, x, y + r, h - 2 * r, c);
        draw_vline(g, x + w - 1, y + r, h - 2 * r, c);
        int f = 1 - r;
        int ddfx = 1;
        int ddfy = -2 * r;
        int px = 0;
        int py = r;
        int cx0 = x + r;
        int cy0 = y + r;
        int cx1 = x + w - 1 - r;
        int cy1 = y + h - 1 - r;
        draw_pixel(g, cx0, cy0 - r, c);
        draw_pixel(g, cx0, cy1 + r, c);
        draw_pixel(g, cx1, cy0 - r, c);
        draw_pixel(g, cx1, cy1 + r, c);
        draw_pixel(g, cx0 - r, cy0, c);
        draw_pixel(g, cx0 + r, cy0, c);
        draw_pixel(g, cx1 - r, cy1, c);
        draw_pixel(g, cx1 + r, cy1, c);
        while (px < py) {
            if (f >= 0) {
                py--;
                ddfy += 2;
                f += ddfy;
            }
            px++;
            ddfx += 2;
            f += ddfx;
            draw_pixel(g, cx0 - px, cy0 - py, c);
            draw_pixel(g, cx0 - py, cy0 - px, c);
            draw_pixel(g, cx1 + px, cy0 - py, c);
            draw_pixel(g, cx1 + py, cy0 - px, c);
            draw_pixel(g, cx0 - px, cy1 + py, c);
            draw_pixel(g, cx0 - py, cy1 + px, c);
            draw_pixel(g, cx1 + px, cy1 + py, c);
            draw_pixel(g, cx1 + py, cy1 + px, c);
        }
    }
}

void draw_elevated_rect(Gfx *g, int x, int y, int w, int h, int r, uint16_t fill, uint16_t border)
{
    if (!g || w <= 0 || h <= 0)
        return;
    uint16_t shadow = RGB565(212, 197, 176);
    draw_round_rect(g, x + 1, y + 2, w, h, r, shadow, 1);
    draw_round_rect(g, x, y, w, h, r, fill, 1);
    draw_round_rect(g, x, y, w, h, r, border, 0);
}

void draw_pixel_aa(Gfx *g, int x, int y, uint16_t c, uint8_t cover)
{
    if (!g || cover == 0)
        return;
    if (cover >= 255) {
        draw_pixel(g, x, y, c);
        return;
    }
    if (x < 0 || y < 0 || x >= g->w || y >= g->h)
        return;
    uint32_t src = compose_rgb565_to_argb(c);
    src = (src & 0x00FFFFFFu) | ((uint32_t)cover << 24);
    if (g->composer && g->composer->px) {
        uint32_t *p = &g->composer->px[y * g->composer->stride + x];
        *p = compose_blend(*p, src);
        g->back[y * g->w + x] = compose_argb_to_rgb565(*p);
    } else {
        uint16_t dst = g->back[y * g->w + x];
        int r1 = (dst >> 11) & 0x1F, g1 = (dst >> 5) & 0x3F, b1 = dst & 0x1F;
        int r2 = (c >> 11) & 0x1F, g2 = (c >> 5) & 0x3F, b2 = c & 0x1F;
        int a = cover;
        int r = (r1 * (255 - a) + r2 * a) / 255;
        int gg = (g1 * (255 - a) + g2 * a) / 255;
        int b = (b1 * (255 - a) + b2 * a) / 255;
        g->back[y * g->w + x] = (uint16_t)((r << 11) | (gg << 5) | b);
    }
    gfx_dirty(g, x, y, 1, 1);
}
