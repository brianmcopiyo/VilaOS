#include "gfx_loader.h"
#include "gfx_driver.h"
#include "compose.h"
#include "hwinfo.h"
#include "ttfont.h"
#include "util.h"
#include "fb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#endif

typedef const vilaos_gfx_driver_t *(*GfxDriverEntryFn)(void);

static int read_active_id(char *id, size_t n)
{
    FILE *f = fopen(VILAOS_GFX_DRIVERS_ROOT "/active", "r");
    if (!f)
        return -1;
    if (!fgets(id, (int)n, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    size_t len = strlen(id);
    while (len > 0 && (id[len - 1] == '\n' || id[len - 1] == '\r' || id[len - 1] == ' '))
        id[--len] = 0;
    return id[0] ? 0 : -1;
}

static const vilaos_gfx_driver_t *try_dlopen_driver(Gfx *g, const char *id)
{
#if !defined(_WIN32) && !defined(PHONE_VFB)
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s/libvilaos_gfx.so", VILAOS_GFX_DRIVERS_ROOT, id);
    void *h = dlopen(path, RTLD_NOW);
    if (!h)
        return NULL;
    union {
        void *p;
        GfxDriverEntryFn f;
    } u;
    u.p = dlsym(h, "vilaos_gfx_driver_entry");
    GfxDriverEntryFn entry = u.f;
    if (!entry) {
        u.p = dlsym(h, "vilaos_gfx_builtin_driver");
        entry = u.f;
    }
    if (!entry) {
        dlclose(h);
        return NULL;
    }
    const vilaos_gfx_driver_t *drv = entry();
    if (!drv || drv->abi_version != VILAOS_GFX_ABI_VERSION || !drv->init) {
        dlclose(h);
        return NULL;
    }
    g->drv_dl = h;
    return drv;
#else
    (void)g;
    (void)id;
    return NULL;
#endif
}

int gfx_loader_init(Gfx *g, int desktop)
{
    if (!g)
        return -1;
    HwProfile hw;
    hwinfo_detect(&hw);
    g->cores = hw.cores;
    g->ram_mb = hw.ram_mb;
    g->ram_tier = (int)hw.tier;
    g->workers = hw.workers;

    Composer *comp = (Composer *)calloc(1, sizeof(Composer));
    if (!comp)
        return -1;
    if (compose_init(comp, 640, 480, &hw) != 0) {
        free(comp);
        return -1;
    }
    g->composer = comp;

    const vilaos_gfx_driver_t *drv = NULL;
    char active[64];
    active[0] = 0;
    if (read_active_id(active, sizeof(active)) == 0)
        drv = try_dlopen_driver(g, active);
    if (!drv)
        drv = try_dlopen_driver(g, "builtin");
    if (!drv)
        drv = vilaos_gfx_builtin_driver();

    g->drv = drv;
    str_copy(g->drv_name, sizeof(g->drv_name), drv->name ? drv->name : "builtin");
    str_copy(g->drv_version, sizeof(g->drv_version), drv->version ? drv->version : "1.0.0");

    if (drv->init(g, desktop) != 0) {
#if !defined(_WIN32) && !defined(PHONE_VFB)
        if (g->drv_dl) {
            dlclose(g->drv_dl);
            g->drv_dl = NULL;
        }
#endif
        drv = vilaos_gfx_builtin_driver();
        g->drv = drv;
        str_copy(g->drv_name, sizeof(g->drv_name), drv->name);
        str_copy(g->drv_version, sizeof(g->drv_version), drv->version);
        if (drv->init(g, desktop) != 0) {
            compose_shutdown(comp);
            free(comp);
            g->composer = NULL;
            return -1;
        }
    }

    if (g->composer && (g->w != g->composer->w || g->h != g->composer->h))
        compose_resize(g->composer, g->w, g->h);
    g->compose_px = g->composer ? g->composer->px : NULL;
    g->compose_pixels = g->composer ? g->composer->pixels : 0;
    font_system_init(&hw);
    return 0;
}

void gfx_loader_shutdown(Gfx *g)
{
    if (!g)
        return;
    font_system_shutdown();
    if (g->drv && g->drv->shutdown)
        g->drv->shutdown(g);
#if !defined(_WIN32) && !defined(PHONE_VFB)
    if (g->drv_dl) {
        dlclose(g->drv_dl);
        g->drv_dl = NULL;
    }
#endif
    if (g->composer) {
        compose_shutdown(g->composer);
        free(g->composer);
        g->composer = NULL;
    }
    g->compose_px = NULL;
    g->compose_pixels = 0;
    g->drv = NULL;
}

const vilaos_gfx_driver_t *gfx_loader_driver(const Gfx *g)
{
    return g ? g->drv : NULL;
}

void gfx_loader_status(const Gfx *g, char *buf, size_t n)
{
    if (!buf || n == 0)
        return;
    if (!g) {
        buf[0] = 0;
        return;
    }
    snprintf(buf, n, "%s %s [%s] cores=%d ram=%ldMB tier=%s workers=%d",
             g->drv_name[0] ? g->drv_name : "gfx",
             g->drv_version[0] ? g->drv_version : "?",
             g->backend[0] ? g->backend : "sw",
             g->cores,
             g->ram_mb,
             hwinfo_tier_name((HwRamTier)g->ram_tier),
             g->workers);
}
