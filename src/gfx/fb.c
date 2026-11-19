#include "fb.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if !defined(PHONE_DESKTOP) && !defined(PHONE_VFB)
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#endif

#ifdef PHONE_DESKTOP
#include <SDL2/SDL.h>
static SDL_Window *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture *g_tex;
static uint32_t *g_argb;
static size_t g_argb_pixels;
#endif

#ifdef PHONE_VFB
static char g_vfb_path[MAX_PATH];
#endif

static void dirty_expand(DirtyRect *d, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    int x1 = x + w;
    int y1 = y + h;
    if (!d->valid) {
        d->x0 = x;
        d->y0 = y;
        d->x1 = x1;
        d->y1 = y1;
        d->valid = 1;
        return;
    }
    if (x < d->x0)
        d->x0 = x;
    if (y < d->y0)
        d->y0 = y;
    if (x1 > d->x1)
        d->x1 = x1;
    if (y1 > d->y1)
        d->y1 = y1;
}

static void gfx_update_metrics(Gfx *g)
{
    int short_edge = g->w < g->h ? g->w : g->h;
    int scale = short_edge / REF_W;
    if (scale < 1)
        scale = 1;
    if (scale > 4)
        scale = 4;
    g->font_scale = scale;
    g->status_h = clamp_i(g->h / 16, 18, 48);
    g->softkey_h = clamp_i(g->h / 14, 22, 56);
    g->pad = clamp_i(g->w / 40, 4, 16);
    g->item_h = clamp_i(g->h / 12, 24, 48);
}

int gfx_set_size(Gfx *g, int w, int h)
{
    if (!g || w < 64 || h < 64)
        return -1;
    if (w > MAX_FB_W)
        w = MAX_FB_W;
    if (h > MAX_FB_H)
        h = MAX_FB_H;
    size_t need = (size_t)w * (size_t)h;
    if (need > g->back_pixels || !g->back) {
        uint16_t *nb = (uint16_t *)realloc(g->back, need * sizeof(uint16_t));
        if (!nb)
            return -1;
        g->back = nb;
        g->back_pixels = need;
    }
    if (g->w != w || g->h != h)
        g->resized = 1;
    g->w = w;
    g->h = h;
    gfx_update_metrics(g);
#ifdef PHONE_DESKTOP
    if (g->desktop) {
        if (need > g_argb_pixels || !g_argb) {
            uint32_t *na = (uint32_t *)realloc(g_argb, need * sizeof(uint32_t));
            if (!na)
                return -1;
            g_argb = na;
            g_argb_pixels = need;
        }
        if (g_tex)
            SDL_DestroyTexture(g_tex);
        g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    }
#endif
    return 0;
}

void gfx_dirty(Gfx *g, int x, int y, int w, int h)
{
    if (!g)
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
    dirty_expand(&g->dirty, x, y, w, h);
}

void gfx_dirty_all(Gfx *g)
{
    if (!g)
        return;
    g->dirty.x0 = 0;
    g->dirty.y0 = 0;
    g->dirty.x1 = g->w;
    g->dirty.y1 = g->h;
    g->dirty.valid = 1;
}

uint16_t *gfx_pixels(Gfx *g)
{
    return g ? g->back : NULL;
}

void gfx_clear(Gfx *g, uint16_t color)
{
    if (!g || !g->back)
        return;
    size_t n = (size_t)g->w * (size_t)g->h;
    for (size_t i = 0; i < n; i++)
        g->back[i] = color;
    gfx_dirty_all(g);
}

const char *gfx_format_label(Gfx *g, char *buf, size_t buf_sz)
{
    if (!g || !buf || buf_sz == 0)
        return "";
    const char *fmt = "RGB565";
    if (g->fb_bpp == 32)
        fmt = g->is_xrgb ? "XRGB8888" : "ARGB8888";
    else if (g->fb_bpp == 24)
        fmt = "RGB888";
    else if (g->fb_bpp == 16)
        fmt = "RGB565";
    snprintf(buf, buf_sz, "FB %dx%d %s", g->w, g->h, fmt);
    return buf;
}

#if !defined(PHONE_DESKTOP) && !defined(PHONE_VFB)
static void gfx_fill_hw(Gfx *g, uint32_t color32, uint16_t color16)
{
    if (!g->fb_map)
        return;
    uint8_t *base = (uint8_t *)g->fb_map;
    if (g->is_xrgb || g->fb_bpp == 32) {
        for (int y = 0; y < g->fb_h; y++) {
            uint32_t *row = (uint32_t *)(base + y * g->fb_stride);
            for (int x = 0; x < g->fb_w; x++)
                row[x] = color32;
        }
    } else {
        for (int y = 0; y < g->fb_h; y++) {
            uint16_t *row = (uint16_t *)(base + y * g->fb_stride);
            for (int x = 0; x < g->fb_w; x++)
                row[x] = color16;
        }
    }
}

static int open_fbdev(Gfx *g)
{
    g->fb_fd = open("/dev/fb0", O_RDWR);
    if (g->fb_fd < 0)
        return -1;

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(g->fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(g->fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        close(g->fb_fd);
        g->fb_fd = -1;
        return -1;
    }

    if (vinfo.xres < 64 || vinfo.yres < 64 || vinfo.bits_per_pixel < 16) {
        vinfo.xres = 640;
        vinfo.yres = 480;
        vinfo.xres_virtual = 640;
        vinfo.yres_virtual = 480;
        vinfo.bits_per_pixel = 16;
        vinfo.red.offset = 11;
        vinfo.red.length = 5;
        vinfo.green.offset = 5;
        vinfo.green.length = 6;
        vinfo.blue.offset = 0;
        vinfo.blue.length = 5;
        vinfo.transp.offset = 0;
        vinfo.transp.length = 0;
        ioctl(g->fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
        ioctl(g->fb_fd, FBIOGET_VSCREENINFO, &vinfo);
        ioctl(g->fb_fd, FBIOGET_FSCREENINFO, &finfo);
    }

    g->fb_w = (int)vinfo.xres;
    g->fb_h = (int)vinfo.yres;
    g->fb_bpp = (int)vinfo.bits_per_pixel;
    g->fb_stride = (int)finfo.line_length;
    g->is_xrgb = (g->fb_bpp == 32);
    if (gfx_set_size(g, g->fb_w, g->fb_h) != 0) {
        close(g->fb_fd);
        g->fb_fd = -1;
        return -1;
    }

    g->fb_map_len = (size_t)finfo.smem_len;
    g->fb_map = mmap(NULL, g->fb_map_len, PROT_READ | PROT_WRITE, MAP_SHARED, g->fb_fd, 0);
    if (g->fb_map == MAP_FAILED) {
        close(g->fb_fd);
        g->fb_fd = -1;
        g->fb_map = NULL;
        g->fb_map_len = 0;
        return -1;
    }

    gfx_fill_hw(g, 0xFF000000u, RGB565(0, 0, 0));
    return 0;
}
#endif

#ifdef PHONE_DESKTOP
static int open_sdl(Gfx *g)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return -1;
    g_win = SDL_CreateWindow("VilaOS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             640, 480, SDL_WINDOW_RESIZABLE);
    if (!g_win)
        return -1;
    g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED);
    if (!g_ren)
        return -1;
    g->desktop = 1;
    g->fb_bpp = 32;
    g->is_xrgb = 1;
    int w = 0, h = 0;
    SDL_GetWindowSize(g_win, &w, &h);
    g->fb_w = w;
    g->fb_h = h;
    if (gfx_set_size(g, w, h) != 0)
        return -1;
    return 0;
}
#endif

int gfx_init(Gfx *g, int desktop)
{
    memset(g, 0, sizeof(*g));
    g->fb_fd = -1;
    g->desktop = 0;
    g->font_scale = 1;

#ifdef PHONE_DESKTOP
    (void)desktop;
    if (open_sdl(g) == 0) {
        gfx_clear(g, COL_BG);
        g->resized = 0;
        return 0;
    }
    fprintf(stderr, "SDL desktop backend failed\n");
    return -1;
#elif defined(PHONE_VFB)
    (void)desktop;
    {
        const char *p = getenv("VILAOS_VFB");
        str_copy(g_vfb_path, sizeof(g_vfb_path), p ? p : "vilaos_fb.ppm");
    }
    g->desktop = 1;
    g->fb_bpp = 16;
    g->is_xrgb = 0;
    if (gfx_set_size(g, 640, 480) != 0)
        return -1;
    g->fb_w = g->w;
    g->fb_h = g->h;
    fprintf(stderr, "VFB writing %s (keys via stdin)\n", g_vfb_path);
    gfx_clear(g, COL_BG);
    g->resized = 0;
    return 0;
#else
    (void)desktop;
    if (open_fbdev(g) == 0) {
        gfx_clear(g, COL_BG);
        g->resized = 0;
        return 0;
    }
    fprintf(stderr, "fb0 unavailable, using software buffer only\n");
    g->fb_bpp = 16;
    g->is_xrgb = 0;
    if (gfx_set_size(g, 640, 480) != 0)
        return -1;
    g->fb_w = g->w;
    g->fb_h = g->h;
    gfx_clear(g, COL_BG);
    g->resized = 0;
    return 0;
#endif
}

void gfx_shutdown(Gfx *g)
{
#ifdef PHONE_DESKTOP
    if (g->desktop) {
        free(g_argb);
        g_argb = NULL;
        g_argb_pixels = 0;
        if (g_tex)
            SDL_DestroyTexture(g_tex);
        if (g_ren)
            SDL_DestroyRenderer(g_ren);
        if (g_win)
            SDL_DestroyWindow(g_win);
        SDL_Quit();
        g_tex = NULL;
        g_ren = NULL;
        g_win = NULL;
    }
#endif
#if !defined(PHONE_DESKTOP) && !defined(PHONE_VFB)
    if (g->fb_map && g->fb_map_len) {
        munmap(g->fb_map, g->fb_map_len);
        g->fb_map = NULL;
        g->fb_map_len = 0;
    }
    if (g->fb_fd >= 0) {
        close(g->fb_fd);
        g->fb_fd = -1;
    }
#endif
    free(g->back);
    g->back = NULL;
    g->back_pixels = 0;
}

static uint32_t rgb565_to_xrgb(uint16_t c)
{
    uint32_t r = (c >> 11) & 0x1F;
    uint32_t gv = (c >> 5) & 0x3F;
    uint32_t b = c & 0x1F;
    r = (r << 3) | (r >> 2);
    gv = (gv << 2) | (gv >> 4);
    b = (b << 3) | (b >> 2);
    return 0xFF000000u | (r << 16) | (gv << 8) | b;
}

#ifdef PHONE_VFB
static void vfb_write_ppm(Gfx *g)
{
    FILE *f = fopen(g_vfb_path, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", g->w, g->h);
    for (int i = 0; i < g->w * g->h; i++) {
        uint32_t c = rgb565_to_xrgb(g->back[i]);
        unsigned char rgb[3] = {
            (unsigned char)((c >> 16) & 0xFF),
            (unsigned char)((c >> 8) & 0xFF),
            (unsigned char)(c & 0xFF)
        };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
#endif

void gfx_present(Gfx *g)
{
    if (!g || !g->dirty.valid || !g->back)
        return;

    int x0 = clamp_i(g->dirty.x0, 0, g->w - 1);
    int y0 = clamp_i(g->dirty.y0, 0, g->h - 1);
    int x1 = clamp_i(g->dirty.x1, 1, g->w);
    int y1 = clamp_i(g->dirty.y1, 1, g->h);

#ifdef PHONE_DESKTOP
    if (g->desktop && g_argb && g_tex && g_ren) {
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++)
                g_argb[y * g->w + x] = rgb565_to_xrgb(g->back[y * g->w + x]);
        }
        SDL_UpdateTexture(g_tex, NULL, g_argb, g->w * 4);
        SDL_RenderClear(g_ren);
        SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
        SDL_RenderPresent(g_ren);
        g->dirty.valid = 0;
        return;
    }
#endif

#ifdef PHONE_VFB
    vfb_write_ppm(g);
    g->dirty.valid = 0;
    return;
#endif

#if !defined(PHONE_DESKTOP) && !defined(PHONE_VFB)
    if (!g->fb_map) {
        g->dirty.valid = 0;
        return;
    }

    uint8_t *dst_base = (uint8_t *)g->fb_map;
    int copy_w = g->w < g->fb_w ? g->w : g->fb_w;
    int copy_h = g->h < g->fb_h ? g->h : g->fb_h;
    if (x1 > copy_w)
        x1 = copy_w;
    if (y1 > copy_h)
        y1 = copy_h;

    if (g->is_xrgb || g->fb_bpp == 32) {
        for (int y = y0; y < y1; y++) {
            uint32_t *dst = (uint32_t *)(dst_base + y * g->fb_stride);
            uint16_t *src = &g->back[y * g->w];
            for (int x = x0; x < x1; x++)
                dst[x] = rgb565_to_xrgb(src[x]);
        }
    } else {
        for (int y = y0; y < y1; y++) {
            uint16_t *dst = (uint16_t *)(dst_base + y * g->fb_stride);
            uint16_t *src = &g->back[y * g->w];
            for (int x = x0; x < x1; x++)
                dst[x] = src[x];
        }
    }
#else
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
#endif
    g->dirty.valid = 0;
}

int gfx_poll_desktop(Gfx *g)
{
#ifdef PHONE_DESKTOP
    if (!g || !g->desktop)
        return 0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
            return 1;
        if (e.type == SDL_WINDOWEVENT &&
            (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
             e.window.event == SDL_WINDOWEVENT_RESIZED)) {
            int w = e.window.data1;
            int h = e.window.data2;
            if (w < 64)
                w = 64;
            if (h < 64)
                h = 64;
            g->fb_w = w;
            g->fb_h = h;
            gfx_set_size(g, w, h);
        }
    }
    return 0;
#else
    (void)g;
    return 0;
#endif
}
