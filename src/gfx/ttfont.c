#include "ttfont.h"
#include "draw.h"
#include "fb.h"
#include "linux_gfx.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "../../third_party/stb_truetype.h"

#define TT_WEIGHT_REG 0
#define TT_WEIGHT_BOLD 1
#define TT_CACHE_SLOTS_MIN 64
#define TT_CACHE_SLOTS_MAX 8192

typedef struct {
    unsigned char *data;
    size_t size;
    stbtt_fontinfo info;
    int ready;
} TtFaceFile;

typedef struct {
    uint32_t key;
    int advance;
    int w, h, xoff, yoff;
    int baseline;
    uint8_t *bits;
    size_t bytes;
    uint32_t stamp;
} TtGlyph;

static TtFaceFile g_face[FONT_FACE_COUNT][2];
static TtGlyph *g_cache;
static int g_cache_slots;
static size_t g_cache_budget;
static size_t g_cache_used;
static uint32_t g_stamp;
static int g_ready;

static int is_bold(int weight)
{
    return weight >= 600 ? TT_WEIGHT_BOLD : TT_WEIGHT_REG;
}

static int load_file(TtFaceFile *face, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    long sz = ftell(fp);
    if (sz <= 0 || sz > 8 * 1024 * 1024) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    unsigned char *buf = (unsigned char *)malloc((size_t)sz);
    if (!buf) {
        fclose(fp);
        return -1;
    }
    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    if (!stbtt_InitFont(&face->info, buf, 0)) {
        free(buf);
        return -1;
    }
    face->data = buf;
    face->size = (size_t)sz;
    face->ready = 1;
    return 0;
}

static void try_load(FontFace face, int bold, const char *const *paths)
{
    TtFaceFile *dst = &g_face[face][bold];
    if (dst->ready)
        return;
    for (int i = 0; paths[i]; i++) {
        if (load_file(dst, paths[i]) == 0)
            return;
    }
}

static TtFaceFile *pick_face(FontFace face, int weight)
{
    int bold = is_bold(weight);
    if (g_face[face][bold].ready)
        return &g_face[face][bold];
    if (g_face[face][TT_WEIGHT_REG].ready)
        return &g_face[face][TT_WEIGHT_REG];
    if (g_face[FONT_FACE_UI][TT_WEIGHT_REG].ready)
        return &g_face[FONT_FACE_UI][TT_WEIGHT_REG];
    return NULL;
}

static uint32_t glyph_key(FontFace face, int size_px, int weight, unsigned code)
{
    uint32_t f = (uint32_t)face & 3u;
    uint32_t s = (uint32_t)(size_px & 0xFF);
    uint32_t w = is_bold(weight) ? 1u : 0u;
    uint32_t c = code & 0xFFFFu;
    return (f << 26) | (w << 25) | (s << 17) | c;
}

static void cache_free_slot(TtGlyph *g)
{
    if (!g)
        return;
    if (g->bits) {
        if (g_cache_used >= g->bytes)
            g_cache_used -= g->bytes;
        else
            g_cache_used = 0;
        free(g->bits);
    }
    memset(g, 0, sizeof(*g));
}

static void cache_evict(size_t need)
{
    while (g_cache_used + need > g_cache_budget && g_cache_used > 0) {
        int victim = -1;
        uint32_t oldest = 0xFFFFFFFFu;
        for (int i = 0; i < g_cache_slots; i++) {
            if (!g_cache[i].bits)
                continue;
            if (g_cache[i].stamp < oldest) {
                oldest = g_cache[i].stamp;
                victim = i;
            }
        }
        if (victim < 0)
            break;
        cache_free_slot(&g_cache[victim]);
    }
}

static TtGlyph *cache_lookup(uint32_t key)
{
    if (!g_cache || g_cache_slots <= 0)
        return NULL;
    int idx = (int)(key % (uint32_t)g_cache_slots);
    for (int n = 0; n < g_cache_slots; n++) {
        int i = (idx + n) % g_cache_slots;
        if (g_cache[i].key == key && g_cache[i].bits) {
            g_cache[i].stamp = ++g_stamp;
            return &g_cache[i];
        }
        if (!g_cache[i].bits && g_cache[i].key == 0)
            break;
    }
    return NULL;
}

static TtGlyph *cache_insert(uint32_t key, int advance, int w, int h, int xoff, int yoff, int baseline,
                             unsigned char *bits, size_t bytes)
{
    if (!g_cache || g_cache_slots <= 0) {
        free(bits);
        return NULL;
    }
    cache_evict(bytes);
    int idx = (int)(key % (uint32_t)g_cache_slots);
    int slot = -1;
    for (int n = 0; n < g_cache_slots; n++) {
        int i = (idx + n) % g_cache_slots;
        if (!g_cache[i].bits) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = idx;
        cache_free_slot(&g_cache[slot]);
        cache_evict(bytes);
    }
    TtGlyph *g = &g_cache[slot];
    g->key = key;
    g->advance = advance;
    g->w = w;
    g->h = h;
    g->xoff = xoff;
    g->yoff = yoff;
    g->baseline = baseline;
    g->bits = bits;
    g->bytes = bytes;
    g->stamp = ++g_stamp;
    g_cache_used += bytes;
    return g;
}

static TtGlyph *rasterize(FontFace face, int size_px, int weight, unsigned code)
{
    uint32_t key = glyph_key(face, size_px, weight, code);
    TtGlyph *hit = cache_lookup(key);
    if (hit)
        return hit;
    TtFaceFile *ff = pick_face(face, weight);
    if (!ff)
        return NULL;
    if (size_px < 6)
        size_px = 6;
    if (size_px > 96)
        size_px = 96;
    float scale = stbtt_ScaleForPixelHeight(&ff->info, (float)size_px);
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&ff->info, &ascent, &descent, &line_gap);
    int baseline = (int)(ascent * scale + 0.5f);
    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&ff->info, (int)code, &adv, &lsb);
    int advance = (int)(adv * scale + 0.5f);
    if (advance < 1)
        advance = size_px / 2;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetCodepointBitmapBox(&ff->info, (int)code, scale, scale, &x0, &y0, &x1, &y1);
    int gw = x1 - x0;
    int gh = y1 - y0;
    if (gw < 0)
        gw = 0;
    if (gh < 0)
        gh = 0;
    size_t bytes = (size_t)gw * (size_t)gh;
    if (bytes == 0)
        bytes = 1;
    unsigned char *bits = (unsigned char *)calloc(1, bytes);
    if (!bits)
        return NULL;
    if (gw > 0 && gh > 0)
        stbtt_MakeCodepointBitmap(&ff->info, bits, gw, gh, gw, scale, scale, (int)code);
    return cache_insert(key, advance, gw, gh, x0, y0, baseline, bits, bytes);
}

static void blit_glyph(Gfx *g, TtGlyph *gl, int x, int y, uint16_t fg, uint16_t bg, int opaque)
{
    if (!g || !gl)
        return;
    int bx = x + gl->xoff;
    int by = y + gl->baseline + gl->yoff;
    if (opaque && gl->advance > 0)
        draw_fill(g, x, y, gl->advance, gl->baseline > 0 ? gl->baseline + (gl->h / 4) : gl->h, bg);
    for (int row = 0; row < gl->h; row++) {
        for (int col = 0; col < gl->w; col++) {
            uint8_t a = gl->bits[(size_t)row * (size_t)gl->w + (size_t)col];
            if (a == 0)
                continue;
            if (a >= 250)
                draw_pixel(g, bx + col, by + row, fg);
            else
                draw_pixel_aa(g, bx + col, by + row, fg, a);
        }
    }
    gfx_dirty(g, bx, by, gl->w > 0 ? gl->w : 1, gl->h > 0 ? gl->h : 1);
}

static unsigned next_codepoint(const char **pp)
{
    const unsigned char *p = (const unsigned char *)*pp;
    if (!p || !*p)
        return 0;
    unsigned char c = *p;
    if (c < 0x80) {
        *pp = (const char *)(p + 1);
        return c;
    }
    if ((c & 0xE0) == 0xC0 && p[1]) {
        unsigned cp = ((c & 0x1Fu) << 6) | (p[1] & 0x3Fu);
        *pp = (const char *)(p + 2);
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
        unsigned cp = ((c & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu);
        *pp = (const char *)(p + 3);
        return cp;
    }
    *pp = (const char *)(p + 1);
    return '?';
}

int font_system_init(const HwProfile *hw)
{
    font_system_shutdown();
    linux_font_init();
    size_t budget = 256u * 1024u;
    int slots = TT_CACHE_SLOTS_MIN;
    if (hw) {
        budget = hw->glyph_cache_bytes;
        if (budget < 128u * 1024u)
            budget = 128u * 1024u;
        if (budget > 64u * 1024u * 1024u)
            budget = 64u * 1024u * 1024u;
        slots = (int)(budget / 1024u);
        if (slots < TT_CACHE_SLOTS_MIN)
            slots = TT_CACHE_SLOTS_MIN;
        if (slots > TT_CACHE_SLOTS_MAX)
            slots = TT_CACHE_SLOTS_MAX;
        if (hw->workers > 1 && slots < TT_CACHE_SLOTS_MAX)
            slots = slots + hw->workers * 16;
        if (slots > TT_CACHE_SLOTS_MAX)
            slots = TT_CACHE_SLOTS_MAX;
    }
    g_cache = (TtGlyph *)calloc((size_t)slots, sizeof(TtGlyph));
    if (!g_cache)
        return -1;
    g_cache_slots = slots;
    g_cache_budget = budget;
    g_cache_used = 0;
    g_stamp = 1;

    const char *inter_reg[] = {
        "/assets/fonts/Inter-Regular.ttf",
        "assets/fonts/Inter-Regular.ttf",
        "fonts/Inter-Regular.ttf",
        "res/fonts/Inter-Regular.ttf",
        NULL
    };
    const char *inter_bold[] = {
        "/assets/fonts/Inter-Bold.ttf",
        "assets/fonts/Inter-Bold.ttf",
        "fonts/Inter-Bold.ttf",
        "res/fonts/Inter-Bold.ttf",
        NULL
    };
    const char *dm_reg[] = {
        "/assets/fonts/DMSans-Regular.ttf",
        "assets/fonts/DMSans-Regular.ttf",
        "fonts/DMSans-Regular.ttf",
        "res/fonts/DMSans-Regular.ttf",
        NULL
    };
    const char *dm_bold[] = {
        "/assets/fonts/DMSans-Bold.ttf",
        "assets/fonts/DMSans-Bold.ttf",
        "fonts/DMSans-Bold.ttf",
        "res/fonts/DMSans-Bold.ttf",
        NULL
    };
    const char *mono_reg[] = {
        "/assets/fonts/JetBrainsMono-Regular.ttf",
        "assets/fonts/JetBrainsMono-Regular.ttf",
        "fonts/JetBrainsMono-Regular.ttf",
        "res/fonts/JetBrainsMono-Regular.ttf",
        NULL
    };
    const char *mono_bold[] = {
        "/assets/fonts/JetBrainsMono-Bold.ttf",
        "assets/fonts/JetBrainsMono-Bold.ttf",
        "fonts/JetBrainsMono-Bold.ttf",
        "res/fonts/JetBrainsMono-Bold.ttf",
        NULL
    };

    try_load(FONT_FACE_UI, TT_WEIGHT_REG, inter_reg);
    try_load(FONT_FACE_UI, TT_WEIGHT_BOLD, inter_bold);
    try_load(FONT_FACE_DISPLAY, TT_WEIGHT_REG, dm_reg);
    try_load(FONT_FACE_DISPLAY, TT_WEIGHT_BOLD, dm_bold);
    try_load(FONT_FACE_MONO, TT_WEIGHT_REG, mono_reg);
    try_load(FONT_FACE_MONO, TT_WEIGHT_BOLD, mono_bold);

    g_ready = g_face[FONT_FACE_UI][TT_WEIGHT_REG].ready ||
              g_face[FONT_FACE_DISPLAY][TT_WEIGHT_REG].ready ||
              g_face[FONT_FACE_MONO][TT_WEIGHT_REG].ready;
    return 0;
}

void font_system_shutdown(void)
{
    linux_font_shutdown();
    if (g_cache) {
        for (int i = 0; i < g_cache_slots; i++)
            cache_free_slot(&g_cache[i]);
        free(g_cache);
        g_cache = NULL;
    }
    g_cache_slots = 0;
    g_cache_budget = 0;
    g_cache_used = 0;
    for (int f = 0; f < FONT_FACE_COUNT; f++) {
        for (int w = 0; w < 2; w++) {
            free(g_face[f][w].data);
            memset(&g_face[f][w], 0, sizeof(g_face[f][w]));
        }
    }
    g_ready = 0;
}

int font_system_ready(void)
{
    return g_ready;
}

size_t font_system_cache_bytes(void)
{
    return g_cache_budget;
}

size_t font_system_cache_used(void)
{
    return g_cache_used;
}

FontFace font_face_from_name(const char *name)
{
    if (!name || !name[0])
        return FONT_FACE_UI;
    if (str_eq(name, "Inter") || str_eq(name, "inter") || str_eq(name, "ui") || str_eq(name, "sans"))
        return FONT_FACE_UI;
    if (str_eq(name, "DM Sans") || str_eq(name, "DMSans") || str_eq(name, "dm-sans") ||
        str_eq(name, "display") || str_eq(name, "IBM Plex Sans") || str_eq(name, "Plex Sans"))
        return FONT_FACE_DISPLAY;
    if (str_eq(name, "JetBrains Mono") || str_eq(name, "JetBrainsMono") || str_eq(name, "mono") ||
        str_eq(name, "IBM Plex Mono") || str_eq(name, "Plex Mono") || str_eq(name, "monospace"))
        return FONT_FACE_MONO;
    return FONT_FACE_UI;
}

FontStyle font_style_default(FontFace face, int size_px, int weight)
{
    FontStyle s;
    s.face = face;
    s.size_px = size_px > 0 ? size_px : 12;
    s.weight = weight > 0 ? weight : 400;
    return s;
}

FontStyle font_style_from_widget(const Widget *w, int fallback_size)
{
    FontStyle s;
    s.face = FONT_FACE_UI;
    s.size_px = fallback_size > 0 ? fallback_size : 12;
    s.weight = 400;
    if (!w)
        return s;
    if (w->font_family[0])
        s.face = font_face_from_name(w->font_family);
    if (w->font_size > 0)
        s.size_px = w->font_size;
    if (w->font_weight > 0)
        s.weight = w->font_weight;
    return s;
}

int font_tt_available(FontFace face, int weight)
{
    return pick_face(face, weight) != NULL;
}

int font_tt_char_h(const FontStyle *style)
{
    if (!style)
        return 12;
    return style->size_px > 0 ? style->size_px : 12;
}

int font_tt_text_w(const FontStyle *style, const char *text)
{
    if (!style || !text || !g_ready)
        return 0;
    int w = 0;
    const char *p = text;
    while (*p) {
        unsigned cp = next_codepoint(&p);
        if (!cp)
            break;
        TtGlyph *gl = rasterize(style->face, style->size_px, style->weight, cp);
        if (gl)
            w += gl->advance;
        else
            w += style->size_px / 2;
    }
    return w;
}

void font_tt_draw_text(Gfx *g, const FontStyle *style, int x, int y, const char *text,
                       uint16_t fg, uint16_t bg, int opaque)
{
    if (!g || !style || !text)
        return;
    if (linux_font_ready()) {
        const char *path = "/assets/fonts/Inter-Regular.ttf";
        if (style->face == FONT_FACE_DISPLAY)
            path = "/assets/fonts/Inter-Bold.ttf";
        else if (style->face == FONT_FACE_MONO)
            path = "/assets/fonts/JetBrainsMono-Regular.ttf";
        if (style->weight >= 600)
            path = "/assets/fonts/Inter-Bold.ttf";
        linux_font_draw(g, path, style->size_px, style->weight, x, y, text, fg, bg, opaque, 0);
        return;
    }
    if (!g_ready)
        return;
    int cx = x;
    const char *p = text;
    while (*p) {
        unsigned cp = next_codepoint(&p);
        if (!cp)
            break;
        TtGlyph *gl = rasterize(style->face, style->size_px, style->weight, cp);
        if (!gl) {
            cx += style->size_px / 2;
            continue;
        }
        blit_glyph(g, gl, cx, y, fg, bg, opaque);
        cx += gl->advance;
    }
}

void font_tt_draw_text_clip(Gfx *g, const FontStyle *style, int x, int y, int max_w, const char *text,
                            uint16_t fg, uint16_t bg, int opaque)
{
    if (!g || !style || !text || max_w <= 0 || !g_ready)
        return;
    if (x < 0) {
        max_w += x;
        x = 0;
    }
    if (x + max_w > g->w)
        max_w = g->w - x;
    if (max_w <= 0)
        return;

    int full_w = font_tt_text_w(style, text);
    int need_ellipsis = full_w > max_w;
    int dots_w = 0;
    if (need_ellipsis) {
        TtGlyph *dot = rasterize(style->face, style->size_px, style->weight, '.');
        dots_w = dot ? dot->advance * 3 : style->size_px;
        if (dots_w >= max_w) {
            int cx = x;
            for (int i = 0; i < 3 && cx < x + max_w; i++) {
                TtGlyph *gl = rasterize(style->face, style->size_px, style->weight, '.');
                if (!gl)
                    break;
                if (cx + gl->advance > x + max_w)
                    break;
                blit_glyph(g, gl, cx, y, fg, bg, opaque);
                cx += gl->advance;
            }
            return;
        }
    }
    int budget = need_ellipsis ? max_w - dots_w : max_w;
    int cx = x;
    const char *p = text;
    while (*p) {
        const char *save = p;
        unsigned cp = next_codepoint(&p);
        if (!cp)
            break;
        TtGlyph *gl = rasterize(style->face, style->size_px, style->weight, cp);
        int adv = gl ? gl->advance : style->size_px / 2;
        if (cx + adv > x + budget) {
            p = save;
            break;
        }
        if (gl)
            blit_glyph(g, gl, cx, y, fg, bg, opaque);
        cx += adv;
    }
    if (need_ellipsis) {
        for (int i = 0; i < 3; i++) {
            TtGlyph *gl = rasterize(style->face, style->size_px, style->weight, '.');
            if (!gl || cx + gl->advance > x + max_w)
                break;
            blit_glyph(g, gl, cx, y, fg, bg, opaque);
            cx += gl->advance;
        }
    }
}
