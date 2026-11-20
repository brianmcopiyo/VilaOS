#include "linux_gfx.h"
#include "fb.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(VILAOS_HAVE_LINUX_GFX) && !defined(_WIN32)

#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library g_ft;
static int g_ft_ok;

int linux_font_init(void)
{
    if (g_ft_ok)
        return 0;
    if (FT_Init_FreeType(&g_ft) != 0)
        return -1;
    g_ft_ok = 1;
    return 0;
}

void linux_font_shutdown(void)
{
    if (!g_ft_ok)
        return;
    FT_Done_FreeType(g_ft);
    g_ft = NULL;
    g_ft_ok = 0;
}

int linux_font_ready(void)
{
    return g_ft_ok;
}

static FT_Face open_face(const char *path, int size_px, int weight)
{
    FT_Face face = NULL;
    if (!g_ft_ok || !path || !path[0])
        return NULL;
    if (FT_New_Face(g_ft, path, 0, &face) != 0)
        return NULL;
    FT_Set_Pixel_Sizes(face, 0, size_px > 0 ? (FT_UInt)size_px : 16);
    if (weight >= 600 && face->num_faces > 1)
        FT_Set_Char_Size(face, 0, (size_px > 0 ? size_px : 16) * 64, 96, 96);
    return face;
}

int linux_font_char_h(int size_px)
{
    return size_px > 0 ? size_px + 4 : 20;
}

int linux_font_text_w(const char *path, int size_px, int weight, const char *text)
{
    FT_Face face = open_face(path, size_px, weight);
    if (!face || !text)
        return 0;
    int w = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (FT_Load_Char(face, *p, FT_LOAD_DEFAULT) != 0)
            continue;
        w += (int)(face->glyph->advance.x >> 6);
    }
    FT_Done_Face(face);
    return w;
}

void linux_font_draw(Gfx *g, const char *path, int size_px, int weight, int x, int y,
                     const char *text, uint16_t fg, uint16_t bg, int opaque, int max_w)
{
    if (!g || !g->back || !text)
        return;
    FT_Face face = open_face(path, size_px, weight);
    if (!face)
        return;
    int pen_x = x;
    int baseline = y + (size_px > 0 ? size_px : 16);
    int fr = ((fg >> 11) & 0x1F) * 255 / 31;
    int fg_g = ((fg >> 5) & 0x3F) * 255 / 63;
    int fb = (fg & 0x1F) * 255 / 31;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (FT_Load_Char(face, *p, FT_LOAD_RENDER) != 0)
            continue;
        FT_GlyphSlot slot = face->glyph;
        int gw = (int)slot->bitmap.width;
        int gh = (int)slot->bitmap.rows;
        int ox = pen_x + slot->bitmap_left;
        int oy = baseline - slot->bitmap_top;
        if (max_w > 0 && pen_x - x + (int)(slot->advance.x >> 6) > max_w)
            break;
        for (int row = 0; row < gh; row++) {
            for (int col = 0; col < gw; col++) {
                int px = ox + col;
                int py = oy + row;
                if (px < 0 || py < 0 || px >= g->w || py >= g->h)
                    continue;
                uint8_t cover = slot->bitmap.buffer[row * (int)slot->bitmap.pitch + col];
                if (cover == 0)
                    continue;
                if (opaque && cover >= 200) {
                    g->back[py * g->w + px] = fg;
                } else if (linux_gfx_ready(g)) {
                    linux_gfx_pixel_aa(g, px, py, fg, cover);
                } else {
                    uint16_t dst = g->back[py * g->w + px];
                    if (opaque)
                        dst = bg;
                    int dr = ((dst >> 11) & 0x1F) * 255 / 31;
                    int dg = ((dst >> 5) & 0x3F) * 255 / 63;
                    int db = (dst & 0x1F) * 255 / 31;
                    int a = cover;
                    int r = (dr * (255 - a) + fr * a) / 255;
                    int gg = (dg * (255 - a) + fg_g * a) / 255;
                    int b = (db * (255 - a) + fb * a) / 255;
                    g->back[py * g->w + px] = RGB565(r, gg, b);
                    gfx_dirty(g, px, py, 1, 1);
                }
            }
        }
        pen_x += (int)(slot->advance.x >> 6);
    }
    FT_Done_Face(face);
}

#else

int linux_font_init(void)
{
    return -1;
}
void linux_font_shutdown(void)
{
}
int linux_font_ready(void)
{
    return 0;
}
int linux_font_text_w(const char *path, int size_px, int weight, const char *text)
{
    (void)path;
    (void)size_px;
    (void)weight;
    (void)text;
    return 0;
}
int linux_font_char_h(int size_px)
{
    return size_px > 0 ? size_px + 4 : 20;
}
void linux_font_draw(Gfx *g, const char *path, int size_px, int weight, int x, int y,
                     const char *text, uint16_t fg, uint16_t bg, int opaque, int max_w)
{
    (void)g;
    (void)path;
    (void)size_px;
    (void)weight;
    (void)x;
    (void)y;
    (void)text;
    (void)fg;
    (void)bg;
    (void)opaque;
    (void)max_w;
}

#endif
