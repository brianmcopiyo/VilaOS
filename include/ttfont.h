#ifndef VILAOS_TTFONT_H
#define VILAOS_TTFONT_H

#include "types.h"
#include "hwinfo.h"

typedef enum {
    FONT_FACE_UI = 0,
    FONT_FACE_DISPLAY = 1,
    FONT_FACE_MONO = 2,
    FONT_FACE_COUNT = 3
} FontFace;

typedef struct {
    FontFace face;
    int size_px;
    int weight;
} FontStyle;

int font_system_init(const HwProfile *hw);
void font_system_shutdown(void);
int font_system_ready(void);
size_t font_system_cache_bytes(void);
size_t font_system_cache_used(void);

FontFace font_face_from_name(const char *name);
FontStyle font_style_default(FontFace face, int size_px, int weight);
FontStyle font_style_from_widget(const Widget *w, int fallback_size);

int font_tt_available(FontFace face, int weight);
int font_tt_char_h(const FontStyle *style);
int font_tt_text_w(const FontStyle *style, const char *text);
void font_tt_draw_text(Gfx *g, const FontStyle *style, int x, int y, const char *text,
                       uint16_t fg, uint16_t bg, int opaque);
void font_tt_draw_text_clip(Gfx *g, const FontStyle *style, int x, int y, int max_w, const char *text,
                            uint16_t fg, uint16_t bg, int opaque);

#endif
