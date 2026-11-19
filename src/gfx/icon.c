#include "icon.h"
#include "draw.h"
#include "hwinfo.h"
#include "util.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NANOSVG_IMPLEMENTATION
#include "../../third_party/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "../../third_party/nanosvgrast.h"

#define ICON_MAX_ENTRIES 256
#define ICON_MAX_SIZE 96
#define ICON_MAX_NAME 64
#define ICON_MAX_SET 32

typedef struct {
    char set[ICON_MAX_SET];
    char name[ICON_MAX_NAME];
    int size;
    uint16_t color;
    uint8_t *rgba;
    size_t bytes;
    uint64_t used;
    int valid;
} IconEntry;

static IconEntry g_cache[ICON_MAX_ENTRIES];
static int g_cache_n;
static size_t g_cache_bytes;
static size_t g_cache_limit;
static uint64_t g_clock;
static NSVGrasterizer *g_rast;
static int g_ready;

static void rgb565_to_rgb(uint16_t c, int *r, int *g, int *b)
{
    *r = ((c >> 11) & 0x1F) * 255 / 31;
    *g = ((c >> 5) & 0x3F) * 255 / 63;
    *b = (c & 0x1F) * 255 / 31;
}

static void normalize_name(const char *in, char *out, size_t n)
{
    size_t j = 0;
    if (!in || !out || n == 0)
        return;
    for (size_t i = 0; in[i] && j + 1 < n; i++) {
        unsigned char ch = (unsigned char)in[i];
        if (ch == '_' || ch == ' ')
            out[j++] = '-';
        else
            out[j++] = (char)tolower(ch);
    }
    out[j] = 0;
}

const char *icon_alias(const char *name, const char *set)
{
    static char mapped[ICON_MAX_NAME];
    char n[ICON_MAX_NAME];
    char s[ICON_MAX_SET];
    normalize_name(name ? name : "", n, sizeof(n));
    normalize_name(set && set[0] ? set : "lucide", s, sizeof(s));
    if (!n[0] || str_eq(n, "app")) {
        if (str_eq(s, "tabler"))
            return "apps";
        if (str_eq(s, "phosphor"))
            return "app-window";
        return "layout-grid";
    }
    if (str_eq(n, "dialer") || str_eq(n, "phone") || str_eq(n, "call")) {
        if (str_eq(s, "phosphor"))
            return "phone";
        return "phone";
    }
    if (str_eq(n, "messages") || str_eq(n, "message") || str_eq(n, "sms")) {
        if (str_eq(s, "tabler"))
            return "message";
        if (str_eq(s, "phosphor"))
            return "chat-circle";
        return "message-square";
    }
    if (str_eq(n, "contacts") || str_eq(n, "people") || str_eq(n, "users")) {
        if (str_eq(s, "phosphor"))
            return "users";
        return "users";
    }
    if (str_eq(n, "notes") || str_eq(n, "note")) {
        if (str_eq(s, "tabler"))
            return "notes";
        if (str_eq(s, "phosphor"))
            return "note";
        return "notebook-pen";
    }
    if (str_eq(n, "calculator") || str_eq(n, "calc"))
        return "calculator";
    if (str_eq(n, "clock") || str_eq(n, "time"))
        return "clock";
    if (str_eq(n, "files") || str_eq(n, "file") || str_eq(n, "folder")) {
        if (str_eq(s, "phosphor"))
            return "folder";
        return "folder";
    }
    if (str_eq(n, "settings") || str_eq(n, "gear")) {
        if (str_eq(s, "tabler"))
            return "settings";
        if (str_eq(s, "phosphor"))
            return "gear";
        return "settings";
    }
    if (str_eq(n, "music") || str_eq(n, "audio")) {
        if (str_eq(s, "phosphor"))
            return "music-notes";
        if (str_eq(s, "tabler"))
            return "music";
        return "music";
    }
    if (str_eq(n, "browser") || str_eq(n, "web")) {
        if (str_eq(s, "tabler"))
            return "world";
        if (str_eq(s, "phosphor"))
            return "globe";
        return "globe";
    }
    str_copy(mapped, sizeof(mapped), n);
    return mapped;
}

static int resolve_path(const char *set, const char *name, char *path, size_t n)
{
    const char *roots[] = {
        "icons",
        "assets/icons",
        "/icons",
        "/assets/icons",
        NULL
    };
    for (int i = 0; roots[i]; i++) {
        snprintf(path, n, "%s/%s/%s.svg", roots[i], set, name);
        FILE *f = fopen(path, "rb");
        if (f) {
            fclose(f);
            return 0;
        }
    }
    return -1;
}

static void cache_evict(size_t need)
{
    while (g_cache_bytes + need > g_cache_limit && g_cache_n > 0) {
        int victim = -1;
        uint64_t oldest = UINT64_MAX;
        for (int i = 0; i < g_cache_n; i++) {
            if (!g_cache[i].valid)
                continue;
            if (g_cache[i].used < oldest) {
                oldest = g_cache[i].used;
                victim = i;
            }
        }
        if (victim < 0)
            break;
        free(g_cache[victim].rgba);
        g_cache_bytes -= g_cache[victim].bytes;
        g_cache[victim] = g_cache[g_cache_n - 1];
        g_cache_n--;
    }
}

static IconEntry *cache_find(const char *set, const char *name, int size, uint16_t color)
{
    for (int i = 0; i < g_cache_n; i++) {
        if (!g_cache[i].valid)
            continue;
        if (g_cache[i].size == size && g_cache[i].color == color &&
            str_eq(g_cache[i].set, set) && str_eq(g_cache[i].name, name)) {
            g_cache[i].used = ++g_clock;
            return &g_cache[i];
        }
    }
    return NULL;
}

static char *load_svg_text(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz <= 0 || sz > 512 * 1024) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = 0;
    if (out_len)
        *out_len = n;
    return buf;
}

static void rewrite_current_color(char *svg, const char *hex)
{
    if (!svg || !hex)
        return;
    char *p = svg;
    while ((p = strstr(p, "currentColor")) != NULL) {
        size_t n = strlen("currentColor");
        size_t h = strlen(hex);
        if (h <= n) {
            memcpy(p, hex, h);
            memmove(p + h, p + n, strlen(p + n) + 1);
            p += h;
        } else {
            break;
        }
    }
    p = svg;
    while ((p = strstr(p, "currentcolor")) != NULL) {
        size_t n = strlen("currentcolor");
        size_t h = strlen(hex);
        if (h <= n) {
            memcpy(p, hex, h);
            memmove(p + h, p + n, strlen(p + n) + 1);
            p += h;
        } else {
            break;
        }
    }
}

static IconEntry *rasterize(const char *set, const char *name, int size, uint16_t color)
{
    char path[MAX_PATH];
    if (resolve_path(set, name, path, sizeof(path)) != 0)
        return NULL;
    char *svg = load_svg_text(path, NULL);
    if (!svg)
        return NULL;
    int cr, cg, cb;
    rgb565_to_rgb(color, &cr, &cg, &cb);
    char hex[16];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", cr, cg, cb);
    rewrite_current_color(svg, hex);

    NSVGimage *image = nsvgParse(svg, "px", 96.0f);
    free(svg);
    if (!image)
        return NULL;
    if (!g_rast) {
        g_rast = nsvgCreateRasterizer();
        if (!g_rast) {
            nsvgDelete(image);
            return NULL;
        }
    }
    float iw = image->width > 1.0f ? image->width : 24.0f;
    float ih = image->height > 1.0f ? image->height : 24.0f;
    float scale = (float)size / (iw > ih ? iw : ih);
    size_t bytes = (size_t)size * (size_t)size * 4u;
    cache_evict(bytes);
    if (g_cache_n >= ICON_MAX_ENTRIES)
        cache_evict(bytes);
    if (g_cache_n >= ICON_MAX_ENTRIES) {
        nsvgDelete(image);
        return NULL;
    }
    uint8_t *rgba = (uint8_t *)calloc(1, bytes);
    if (!rgba) {
        nsvgDelete(image);
        return NULL;
    }
    float tx = ((float)size - iw * scale) * 0.5f;
    float ty = ((float)size - ih * scale) * 0.5f;
    nsvgRasterize(g_rast, image, tx, ty, scale, rgba, size, size, size * 4);
    nsvgDelete(image);

    for (int i = 0; i < size * size; i++) {
        uint8_t *px = rgba + i * 4;
        uint8_t a = px[3];
        if (a == 0) {
            px[0] = px[1] = px[2] = 0;
            continue;
        }
        px[0] = (uint8_t)cr;
        px[1] = (uint8_t)cg;
        px[2] = (uint8_t)cb;
    }

    IconEntry *e = &g_cache[g_cache_n++];
    memset(e, 0, sizeof(*e));
    str_copy(e->set, sizeof(e->set), set);
    str_copy(e->name, sizeof(e->name), name);
    e->size = size;
    e->color = color;
    e->rgba = rgba;
    e->bytes = bytes;
    e->used = ++g_clock;
    e->valid = 1;
    g_cache_bytes += bytes;
    return e;
}

static void draw_fallback(Gfx *g, int cx, int cy, int size, uint16_t fg)
{
    int r = size / 5;
    if (r < 2)
        r = 2;
    if (r > 6)
        r = 6;
    draw_round_rect(g, cx - r, cy - r, r * 2, r * 2, r, fg, 1);
}

void icon_init(Gfx *g)
{
    HwProfile hw;
    hwinfo_detect(&hw);
    g_cache_limit = hw.icon_cache_bytes > 0 ? hw.icon_cache_bytes : 256u * 1024u;
    if (g && g->ram_mb >= 32768)
        g_cache_limit = 48u * 1024u * 1024u;
    else if (g && g->ram_mb >= 8192)
        g_cache_limit = 12u * 1024u * 1024u;
    else if (g && g->ram_mb >= 2048)
        g_cache_limit = 3u * 1024u * 1024u;
    else if (g && g->ram_mb >= 512)
        g_cache_limit = 768u * 1024u;
    else if (g && g->ram_mb > 0)
        g_cache_limit = 256u * 1024u;
    g_ready = 1;
}

void icon_shutdown(void)
{
    for (int i = 0; i < g_cache_n; i++) {
        free(g_cache[i].rgba);
        g_cache[i].rgba = NULL;
        g_cache[i].valid = 0;
    }
    g_cache_n = 0;
    g_cache_bytes = 0;
    if (g_rast) {
        nsvgDeleteRasterizer(g_rast);
        g_rast = NULL;
    }
    g_ready = 0;
}

int icon_draw(Gfx *g, int cx, int cy, int size, const char *name, const char *set, uint16_t fg)
{
    if (!g || size < 8)
        return -1;
    if (size > ICON_MAX_SIZE)
        size = ICON_MAX_SIZE;
    if (!g_ready)
        icon_init(g);

    char setn[ICON_MAX_SET];
    normalize_name(set && set[0] ? set : "lucide", setn, sizeof(setn));
    if (!str_eq(setn, "lucide") && !str_eq(setn, "tabler") && !str_eq(setn, "phosphor"))
        str_copy(setn, sizeof(setn), "lucide");
    const char *resolved = icon_alias(name, setn);

    IconEntry *e = cache_find(setn, resolved, size, fg);
    if (!e)
        e = rasterize(setn, resolved, size, fg);
    if (!e && (str_eq(resolved, "music") || str_eq(name, "music") || str_eq(name, "audio"))) {
        e = cache_find("tabler", "music", size, fg);
        if (!e)
            e = rasterize("tabler", "music", size, fg);
    }
    if (!e && !str_eq(setn, "lucide")) {
        const char *alt = icon_alias(name, "lucide");
        e = cache_find("lucide", alt, size, fg);
        if (!e)
            e = rasterize("lucide", alt, size, fg);
    }
    if (!e && !str_eq(setn, "tabler")) {
        const char *alt = icon_alias(name, "tabler");
        e = cache_find("tabler", alt, size, fg);
        if (!e)
            e = rasterize("tabler", alt, size, fg);
    }
    if (!e) {
        draw_fallback(g, cx, cy, size, fg);
        return 0;
    }

    int x0 = cx - size / 2;
    int y0 = cy - size / 2;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            uint8_t *px = e->rgba + ((size_t)y * (size_t)size + (size_t)x) * 4u;
            uint8_t a = px[3];
            if (a < 8)
                continue;
            draw_pixel_aa(g, x0 + x, y0 + y, fg, a);
        }
    }
    return 1;
}
