#ifndef PHONE_FONT_H
#define PHONE_FONT_H

#include "types.h"
#include "ttfont.h"

typedef enum {
    FONT_8X12 = 0,
    FONT_8X16 = 1,
    FONT_UI = 2,
    FONT_DISPLAY = 3,
    FONT_MONO = 4
} FontId;

int font_char_w(FontId id);
int font_char_h(FontId id);
int font_char_w_scaled(Gfx *g, FontId id);
int font_char_h_scaled(Gfx *g, FontId id);
void font_draw_char(Gfx *g, FontId id, int x, int y, char ch, uint16_t fg, uint16_t bg, int opaque);
void font_draw_char_aa(Gfx *g, FontId id, int x, int y, char ch, uint16_t fg);
void font_draw_text(Gfx *g, FontId id, int x, int y, const char *text, uint16_t fg, uint16_t bg, int opaque);
void font_draw_text_clip(Gfx *g, FontId id, int x, int y, int max_w, const char *text, uint16_t fg, uint16_t bg, int opaque);
int font_text_w(FontId id, const char *text);
int font_text_w_scaled(Gfx *g, FontId id, const char *text);

FontStyle font_style_from_id(Gfx *g, FontId id);
void font_draw_text_style(Gfx *g, const FontStyle *style, int x, int y, const char *text,
                          uint16_t fg, uint16_t bg, int opaque);
void font_draw_text_style_clip(Gfx *g, const FontStyle *style, int x, int y, int max_w, const char *text,
                               uint16_t fg, uint16_t bg, int opaque);
int font_text_w_style(Gfx *g, const FontStyle *style, const char *text);
int font_char_h_style(Gfx *g, const FontStyle *style);

#endif
