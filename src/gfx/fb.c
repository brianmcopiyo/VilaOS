#include "fb.h"
#include "gfx_backend.h"
#include "gfx_loader.h"
#include "compose.h"
#include "linux_gfx.h"
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
    g->font_scale = 1;
    g->aspect_ratio = (g->h > 0) ? ((float)g->w / (float)g->h) : 1.0f;
    g->status_h = 32;
    g->softkey_h = 36;
    g->pad = 12;
    g->item_h = 40;
    if (g->h >= 800) {
        g->status_h = 36;
        g->softkey_h = 40;
        g->pad = 14;
        g->item_h = 44;
    }
    if (g->h < 160) {
        g->status_h = 18;
        g->softkey_h = 22;
        g->item_h = 28;
        g->pad = 6;
    }
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
    if (g->composer)
        compose_resize(g->composer, w, h);
    if (g->composer) {
        g->compose_px = g->composer->px;
        g->compose_pixels = g->composer->pixels;
    }
#if defined(VILAOS_HAVE_LINUX_GFX)
    if (!g->linux_gfx)
        linux_gfx_init(g);
    else
        linux_gfx_resize(g, w, h);
#endif
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
    if (g->composer)
        compose_dirty(g->composer, x, y, w, h);
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
    if (g->composer)
        compose_dirty_all(g->composer);
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
    if (g->composer) {
        compose_begin(g->composer);
        compose_clear(g->composer, compose_rgb565_to_argb(color));
        compose_flush(g->composer, g->back, g->w, g->h);
    }
    gfx_dirty_all(g);
}

const char *gfx_format_label(Gfx *g, char *buf, size_t buf_sz)
{
    if (!g || !buf || buf_sz == 0)
        return "";
#ifndef VILAOS_GFX_SO
    gfx_loader_status(g, buf, buf_sz);
#else
    snprintf(buf, buf_sz, "%s %s [%s] cores=%d ram=%ldMB workers=%d",
             g->drv_name[0] ? g->drv_name : "gfx",
             g->drv_version[0] ? g->drv_version : "?",
             g->backend[0] ? g->backend : "sw",
             g->cores, g->ram_mb, g->workers);
#endif
    return buf;
}

static uint32_t rgb565_to_xrgb(uint16_t c)
{
    return compose_rgb565_to_argb(c);
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

int gfx_backend_probe_fb(Gfx *g)
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
    {
        int lw = g->fb_w;
        int lh = g->fb_h;
        if (lw != PHONE_W || lh != PHONE_H) {
            lw = PHONE_W;
            lh = PHONE_H;
        }
        if (gfx_set_size(g, lw, lh) != 0) {
            close(g->fb_fd);
            g->fb_fd = -1;
            return -1;
        }
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

    str_copy(g->backend, sizeof(g->backend), "fb");
    gfx_fill_hw(g, 0xFF000000u, RGB565(0, 0, 0));
    return 0;
}
#else
int gfx_backend_probe_fb(Gfx *g)
{
    (void)g;
    return -1;
}
#endif

#ifdef PHONE_DESKTOP
int gfx_backend_probe_sdl(Gfx *g)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return -1;
    g_win = SDL_CreateWindow("VilaOS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             PHONE_W, PHONE_H, SDL_WINDOW_RESIZABLE);
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
    str_copy(g->backend, sizeof(g->backend), "sdl");
    if (gfx_set_size(g, w, h) != 0)
        return -1;
    return 0;
}
#else
int gfx_backend_probe_sdl(Gfx *g)
{
    (void)g;
    return -1;
}
#endif

int gfx_backend_init_all(Gfx *g, int desktop)
{
    if (!g)
        return -1;
    g->fb_fd = -1;
    g->dri_fd = -1;
    g->desktop = 0;
    g->font_scale = 1;
    g->backend[0] = 0;

#ifdef PHONE_DESKTOP
    (void)desktop;
    if (gfx_backend_probe_sdl(g) == 0) {
        gfx_clear(g, COL_BG);
        g->resized = 0;
        return 0;
    }
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
    str_copy(g->backend, sizeof(g->backend), "vfb");
    if (gfx_set_size(g, PHONE_W, PHONE_H) != 0)
        return -1;
    g->fb_w = g->w;
    g->fb_h = g->h;
    gfx_clear(g, COL_BG);
    g->resized = 0;
    return 0;
#else
    (void)desktop;
    if (gfx_backend_probe_drm(g) == 0) {
        gfx_clear(g, COL_BG);
        g->resized = 0;
        return 0;
    }
    if (gfx_backend_probe_fb(g) == 0) {
        gfx_clear(g, COL_BG);
        g->resized = 0;
        return 0;
    }
    str_copy(g->backend, sizeof(g->backend), "sw");
    g->fb_bpp = 16;
    g->is_xrgb = 0;
    if (gfx_set_size(g, PHONE_W, PHONE_H) != 0)
        return -1;
    g->fb_w = g->w;
    g->fb_h = g->h;
    gfx_clear(g, COL_BG);
    g->resized = 0;
    return 0;
#endif
}

void gfx_backend_shutdown(Gfx *g)
{
#ifdef PHONE_DESKTOP
    if (g && g->desktop) {
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
    if (g) {
        if (g->fb_map && g->fb_map_len) {
            munmap(g->fb_map, g->fb_map_len);
            g->fb_map = NULL;
            g->fb_map_len = 0;
        }
        if (g->fb_fd >= 0) {
            close(g->fb_fd);
            g->fb_fd = -1;
        }
        if (g->dri_fd >= 0) {
            close(g->dri_fd);
            g->dri_fd = -1;
        }
    }
#endif
    if (g) {
        free(g->back);
        g->back = NULL;
        g->back_pixels = 0;
    }
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

void gfx_backend_present(Gfx *g)
{
    if (!g || !g->back)
        return;

    if (g->composer && g->composer->cmd_count > 0)
        compose_flush(g->composer, g->back, g->w, g->h);

    if (!g->dirty.valid)
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
    static int s_letterbox_ready;
    static int s_lb_w, s_lb_h, s_lb_ox, s_lb_oy, s_lb_dw, s_lb_dh, s_lb_sw, s_lb_sh;
    int need_scale = (g->fb_w != g->w || g->fb_h != g->h);
    if (need_scale) {
        int dst_w = g->fb_w;
        int dst_h = g->fb_h;
        if (g->w > 0 && g->h > 0) {
            long fit_w = (long)g->fb_h * (long)g->w / (long)g->h;
            long fit_h = (long)g->fb_w * (long)g->h / (long)g->w;
            if (fit_w <= g->fb_w) {
                dst_w = (int)fit_w;
                dst_h = g->fb_h;
            } else {
                dst_w = g->fb_w;
                dst_h = (int)fit_h;
            }
        }
        if (dst_w < 1)
            dst_w = 1;
        if (dst_h < 1)
            dst_h = 1;
        int ox = (g->fb_w - dst_w) / 2;
        int oy = (g->fb_h - dst_h) / 2;
        int geometry_changed = !s_letterbox_ready || s_lb_w != g->fb_w || s_lb_h != g->fb_h ||
                               s_lb_sw != g->w || s_lb_sh != g->h || s_lb_dw != dst_w ||
                               s_lb_dh != dst_h || s_lb_ox != ox || s_lb_oy != oy;
        if (geometry_changed) {
            gfx_fill_hw(g, 0xFFF5EFE6u, COL_BG);
            s_letterbox_ready = 1;
            s_lb_w = g->fb_w;
            s_lb_h = g->fb_h;
            s_lb_sw = g->w;
            s_lb_sh = g->h;
            s_lb_dw = dst_w;
            s_lb_dh = dst_h;
            s_lb_ox = ox;
            s_lb_oy = oy;
            x0 = 0;
            y0 = 0;
            x1 = g->w;
            y1 = g->h;
        }
        int dy0 = y0 * dst_h / g->h;
        int dy1 = (y1 * dst_h + g->h - 1) / g->h;
        int dx0 = x0 * dst_w / g->w;
        int dx1 = (x1 * dst_w + g->w - 1) / g->w;
        if (dy0 < 0)
            dy0 = 0;
        if (dx0 < 0)
            dx0 = 0;
        if (dy1 > dst_h)
            dy1 = dst_h;
        if (dx1 > dst_w)
            dx1 = dst_w;
        for (int dy = dy0; dy < dy1; dy++) {
            int sy = dy * g->h / dst_h;
            if (sy < 0)
                sy = 0;
            if (sy >= g->h)
                sy = g->h - 1;
            uint16_t *src = &g->back[sy * g->w];
            if (g->is_xrgb || g->fb_bpp == 32) {
                uint32_t *dst = (uint32_t *)(dst_base + (oy + dy) * g->fb_stride);
                for (int dx = dx0; dx < dx1; dx++) {
                    int sx = dx * g->w / dst_w;
                    if (sx < 0)
                        sx = 0;
                    if (sx >= g->w)
                        sx = g->w - 1;
                    dst[ox + dx] = rgb565_to_xrgb(src[sx]);
                }
            } else {
                uint16_t *dst = (uint16_t *)(dst_base + (oy + dy) * g->fb_stride);
                for (int dx = dx0; dx < dx1; dx++) {
                    int sx = dx * g->w / dst_w;
                    if (sx < 0)
                        sx = 0;
                    if (sx >= g->w)
                        sx = g->w - 1;
                    dst[ox + dx] = src[sx];
                }
            }
        }
    } else if (g->is_xrgb || g->fb_bpp == 32) {
        for (int y = y0; y < y1; y++) {
            uint32_t *dst = (uint32_t *)(dst_base + y * g->fb_stride);
            uint16_t *src = &g->back[y * g->w];
            for (int x = x0; x < x1; x++)
                dst[x] = rgb565_to_xrgb(src[x]);
        }
    } else {
        size_t row_bytes = (size_t)(x1 - x0) * sizeof(uint16_t);
        for (int y = y0; y < y1; y++) {
            uint16_t *dst = (uint16_t *)(dst_base + y * g->fb_stride) + x0;
            uint16_t *src = &g->back[y * g->w + x0];
            memcpy(dst, src, row_bytes);
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

int gfx_backend_poll_desktop(Gfx *g)
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

#ifndef VILAOS_GFX_SO
int gfx_init(Gfx *g, int desktop)
{
    if (!g)
        return -1;
    memset(g, 0, sizeof(*g));
    g->fb_fd = -1;
    g->dri_fd = -1;
    return gfx_loader_init(g, desktop);
}

void gfx_shutdown(Gfx *g)
{
    gfx_loader_shutdown(g);
}
#endif

void gfx_present(Gfx *g)
{
#if defined(VILAOS_HAVE_LINUX_GFX)
    if (g && g->dri_fd >= 0 && linux_drm_present(g) == 0) {
        g->dirty.valid = 0;
        return;
    }
#endif
    if (g && g->drv && g->drv->present)
        g->drv->present(g);
    else
        gfx_backend_present(g);
}

int gfx_poll_desktop(Gfx *g)
{
    return gfx_backend_poll_desktop(g);
}
