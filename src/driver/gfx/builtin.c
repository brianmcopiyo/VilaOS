#include "gfx_driver.h"
#include "gfx_backend.h"
#include "fb.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

static int builtin_init(Gfx *g, int desktop)
{
    return gfx_backend_init_all(g, desktop);
}

static int builtin_query_caps(Gfx *g, vilaos_gfx_caps_t *out)
{
    if (!out)
        return -1;
    memset(out, 0, sizeof(*out));
    out->abi_version = VILAOS_GFX_ABI_VERSION;
    str_copy(out->name, sizeof(out->name), "vilaos-gfx-builtin");
    str_copy(out->version, sizeof(out->version), "1.0.0");
    out->caps = VILAOS_GFX_CAP_AA | VILAOS_GFX_CAP_LAYERS | VILAOS_GFX_CAP_GRADIENT |
                VILAOS_GFX_CAP_FB | VILAOS_GFX_CAP_THREADS;
    if (g && g->desktop)
        out->caps |= VILAOS_GFX_CAP_SDL;
    if (g && (g->gfx_tier == GFX_TIER_DRM || g->gfx_tier == GFX_TIER_GPU ||
              str_eq(g->backend, "drm") || str_starts(g->backend, "gpu")))
        out->caps |= VILAOS_GFX_CAP_DRM;
    out->max_w = MAX_FB_W;
    out->max_h = MAX_FB_H;
    out->work_format = VILAOS_GFX_FMT_ARGB8888;
    out->present_format = (g && (g->is_xrgb || g->fb_bpp == 32)) ? VILAOS_GFX_FMT_XRGB8888
                                                                  : VILAOS_GFX_FMT_RGB565;
    out->max_threads = g && g->workers > 0 ? g->workers : 1;
    return 0;
}

static uint32_t *builtin_acquire(Gfx *g, int *w, int *h, int *stride_px)
{
    if (!g)
        return NULL;
    if (w)
        *w = g->w;
    if (h)
        *h = g->h;
    if (stride_px)
        *stride_px = g->w;
    return g->compose_px;
}

static void builtin_present(Gfx *g)
{
    if (g && (str_eq(g->backend, "headless") || getenv("VILAOS_HEADLESS_UI"))) {
        if (g)
            g->dirty.valid = 0;
        return;
    }
    gfx_backend_present(g);
}

static int builtin_resize(Gfx *g, int w, int h)
{
    return gfx_set_size(g, w, h);
}

static void builtin_shutdown(Gfx *g)
{
    gfx_backend_shutdown(g);
}

static const vilaos_gfx_driver_t g_builtin = {
    .abi_version = VILAOS_GFX_ABI_VERSION,
    .name = "vilaos-gfx-builtin",
    .version = "1.0.0",
    .init = builtin_init,
    .query_caps = builtin_query_caps,
    .acquire_surface = builtin_acquire,
    .present = builtin_present,
    .resize = builtin_resize,
    .shutdown = builtin_shutdown,
};

const vilaos_gfx_driver_t *vilaos_gfx_builtin_driver(void)
{
    return &g_builtin;
}
