#include "vapi.h"
#include "bridge.h"
#include "hwinfo.h"
#include "pkg.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void set_out(VapiCtx *ctx, const char *widget, const char *text, char *out, size_t out_n)
{
    if (widget && widget[0] && ctx->rt)
        bridge_set_text(ctx->rt, widget, text);
    if (out && out_n)
        str_copy(out, out_n, text);
}

static void caps_text(unsigned caps, char *buf, size_t n)
{
    buf[0] = 0;
    if (caps & PKG_CAP_UI)
        str_cat(buf, n, "ui ");
    if (caps & PKG_CAP_STORAGE)
        str_cat(buf, n, "storage.app ");
    if (caps & PKG_CAP_STORAGE_FILES)
        str_cat(buf, n, "storage.files ");
    if (caps & PKG_CAP_TELEPHONY)
        str_cat(buf, n, "telephony ");
    if (caps & PKG_CAP_MESSAGES)
        str_cat(buf, n, "messages ");
    if (caps & PKG_CAP_CONTACTS)
        str_cat(buf, n, "contacts ");
    if (caps & PKG_CAP_AUDIO)
        str_cat(buf, n, "audio.bg ");
    if (caps & PKG_CAP_CLOCK)
        str_cat(buf, n, "system.clock ");
    if (caps & PKG_CAP_SYSTEM_DEVICE)
        str_cat(buf, n, "system.device ");
    if (caps & PKG_CAP_SYSTEM_POWER)
        str_cat(buf, n, "system.power ");
    if (caps & PKG_CAP_NETWORK)
        str_cat(buf, n, "network ");
    if (caps & PKG_CAP_DISPLAY)
        str_cat(buf, n, "display ");
    if (!buf[0])
        str_copy(buf, n, "none");
}

static void load_reg(PkgRegistry *reg)
{
    pkg_registry_load(reg);
    if (reg->count == 0)
        pkg_seed_from_dir("/apps", reg);
}

int vapi_system(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    (void)a2;
    if (!ctx || !op)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "display") || str_eq(op, "display.info")) {
        char buf[MAX_TEXT];
        int w = 0, h = 0;
        if (ctx->gfx) {
            w = ctx->gfx->w;
            h = ctx->gfx->h;
        }
        int g = 0;
        if (h > 0)
            g = (w * 1000) / h;
        snprintf(buf, sizeof(buf), "%dx%d 1:1 px ratio %d.%03d", w, h, g / 1000, g % 1000);
        set_out(ctx, a1, buf, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "graphics") || str_eq(op, "graphics.info") || str_eq(op, "gpu") ||
        str_eq(op, "gpu.info")) {
        char buf[MAX_TEXT];
        const char *tier = "none";
        const char *backend = "sw";
        const char *gpu = "-";
        long vram = 0;
        int accel = 0;
        int w = 0, h = 0;
        if (ctx->gfx) {
            w = ctx->gfx->w;
            h = ctx->gfx->h;
            backend = ctx->gfx->backend[0] ? ctx->gfx->backend : "sw";
            gpu = ctx->gfx->gpu_name[0] ? ctx->gfx->gpu_name : "-";
            vram = ctx->gfx->vram_mb;
            accel = ctx->gfx->gpu_accel;
            if (ctx->gfx->gfx_tier == GFX_TIER_GPU)
                tier = "gpu";
            else if (ctx->gfx->gfx_tier == GFX_TIER_DRM)
                tier = "drm";
            else if (ctx->gfx->gfx_tier == GFX_TIER_FB)
                tier = "fb";
        }
        if (accel)
            snprintf(buf, sizeof(buf), "Tier %s HW accel  %s  %s  %dx%d  VRAM %ldMB", tier, backend,
                     gpu, w, h, vram);
        else
            snprintf(buf, sizeof(buf), "tier %s fallback  %s  %s  %dx%d  VRAM %ldMB", tier, backend,
                     gpu, w, h, vram);
        set_out(ctx, a1, buf, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "device") || str_eq(op, "device.info")) {
        char buf[MAX_TEXT];
        int w = 0, h = 0;
        if (ctx->gfx) {
            w = ctx->gfx->w;
            h = ctx->gfx->h;
        }
        HwProfile hw;
        hwinfo_detect(&hw);
        snprintf(buf, sizeof(buf), "%dx%d %s %ldMB %dc API%d", w, h, hwinfo_tier_name(hw.tier),
                 hw.ram_mb, hw.cores, VILAOS_API_VERSION);
        set_out(ctx, a1, buf, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "memory") || str_eq(op, "memory.info")) {
        char buf[MAX_TEXT];
        HwProfile hw;
        long total_kb = 0, free_kb = 0, avail_kb = 0;
        hwinfo_detect(&hw);
        if (read_meminfo_kb(&total_kb, &free_kb, &avail_kb) != 0) {
            total_kb = hw.ram_kb;
            avail_kb = hw.ram_kb / 2;
        }
        long total_mb = total_kb / 1024;
        long avail_mb = avail_kb / 1024;
        if (total_mb < 1)
            total_mb = hw.ram_mb;
        if (avail_mb < 0)
            avail_mb = 0;
        snprintf(buf, sizeof(buf), "%ld/%ld MB free  %dc  tier %s", avail_mb, total_mb, hw.cores,
                 hwinfo_tier_name(hw.tier));
        set_out(ctx, a1, buf, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "memory.mb") || str_eq(op, "mb")) {
        HwProfile hw;
        long total_kb = 0, free_kb = 0, avail_kb = 0;
        hwinfo_detect(&hw);
        if (read_meminfo_kb(&total_kb, &free_kb, &avail_kb) != 0)
            total_kb = hw.ram_kb;
        long total_mb = total_kb / 1024;
        if (total_mb < 1)
            total_mb = hw.ram_mb;
        if (out && out_n)
            snprintf(out, out_n, "%ld", total_mb);
        return VAPI_OK;
    }
    if (str_eq(op, "storage") || str_eq(op, "storage.info")) {
        char buf[MAX_TEXT];
        PkgRegistry reg;
        load_reg(&reg);
        snprintf(buf, sizeof(buf), "%d pkgs  /apps  /packages  /data/apps", reg.count);
        set_out(ctx, a1, buf, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "input") || str_eq(op, "input.info")) {
        char buf[MAX_TEXT];
        if (ctx->gfx && ctx->gfx->desktop)
            str_copy(buf, sizeof(buf), "Pointer mouse  Keypad  Softkeys");
        else
            str_copy(buf, sizeof(buf), "Keypad  Softkeys  Touch if present");
        set_out(ctx, a1, buf, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "runtime") || str_eq(op, "runtime.info") || str_eq(op, "kernel")) {
        char buf[MAX_TEXT];
        FILE *f = fopen("/proc/version", "r");
        if (f) {
            if (!fgets(buf, (int)sizeof(buf), f))
                buf[0] = 0;
            fclose(f);
            size_t L = strlen(buf);
            while (L > 0 && (buf[L - 1] == '\n' || buf[L - 1] == '\r'))
                buf[--L] = 0;
            if (L > 96)
                buf[96] = 0;
        } else {
            snprintf(buf, sizeof(buf), "VilaOS API%d musl", VILAOS_API_VERSION);
        }
        set_out(ctx, a1, buf, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "packages") || str_eq(op, "packages.list")) {
        char buf[MAX_TEXT];
        char countbuf[32];
        PkgRegistry reg;
        load_reg(&reg);
        if (pkg_list(&reg, buf, sizeof(buf)) < 0)
            str_copy(buf, sizeof(buf), "none");
        if (!buf[0])
            str_copy(buf, sizeof(buf), "none");
        snprintf(countbuf, sizeof(countbuf), "%d", reg.count);
        if (ctx->rt) {
            bridge_set_text(ctx->rt, "pkg_count", countbuf);
            bridge_set_text(ctx->rt, "apps_count", countbuf);
            snprintf(countbuf, sizeof(countbuf), "%d packages", reg.count);
            bridge_set_text(ctx->rt, "pkg_summary", countbuf);
            bridge_set_text(ctx->rt, "apps_summary", countbuf);
            for (int i = 0; i < 10; i++) {
                char wid[8];
                char line[MAX_TEXT];
                snprintf(wid, sizeof(wid), "pkg%d", i);
                if (i < reg.count) {
                    snprintf(line, sizeof(line), "%s %s (%s)", reg.apps[i].name, reg.apps[i].id,
                             reg.apps[i].version);
                    bridge_set_text(ctx->rt, wid, line);
                } else {
                    bridge_set_text(ctx->rt, wid, "— empty");
                }
            }
        }
        set_out(ctx, a1, buf, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "packages.detail") || str_eq(op, "package.detail")) {
        PkgRegistry reg;
        char caps[MAX_TEXT];
        int idx = a1 && a1[0] ? atoi(a1) : 0;
        load_reg(&reg);
        if (idx < 0 || idx >= reg.count)
            return VAPI_ERR_NOT_FOUND;
        caps_text(reg.apps[idx].caps, caps, sizeof(caps));
        if (ctx->rt) {
            bridge_set_text(ctx->rt, "pkg_name", reg.apps[idx].name);
            bridge_set_text(ctx->rt, "pkg_id", reg.apps[idx].id);
            bridge_set_text(ctx->rt, "pkg_ver", reg.apps[idx].version);
            bridge_set_text(ctx->rt, "pkg_caps", caps);
            bridge_set_text(ctx->rt, "pkg_entry", reg.apps[idx].entry_screen);
            bridge_set_text(ctx->rt, "pkg_badge", "Installed .vpkg");
            bridge_set_text(ctx->rt, "pkg_icon", reg.apps[idx].icon);
        }
        set_out(ctx, a2 && a2[0] ? a2 : a1, reg.apps[idx].id, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "time") || str_eq(op, "clock") || str_eq(op, "clock.now")) {
        return vapi_clock(ctx, "now", a1, NULL, out, out_n);
    }
    if (str_eq(op, "locale") || str_eq(op, "locale.get")) {
        set_out(ctx, a1, "en-US", out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "theme") || str_eq(op, "theme.get")) {
        set_out(ctx, a1, "sunset", out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "power.menu") || str_eq(op, "power_menu")) {
        if (!ctx->rt)
            return VAPI_ERR_INTERNAL;
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "assets/screens/power.xml");
        return VAPI_OK;
    }
    if (str_eq(op, "power.off") || str_eq(op, "power_off")) {
        if (!ctx->rt)
            return VAPI_ERR_INTERNAL;
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "power_off");
        return VAPI_OK;
    }
    if (str_eq(op, "power.restart") || str_eq(op, "power_restart")) {
        if (!ctx->rt)
            return VAPI_ERR_INTERNAL;
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "power_restart");
        return VAPI_OK;
    }
    if (str_eq(op, "api.version") || str_eq(op, "version")) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", VILAOS_API_VERSION);
        set_out(ctx, a1, buf, out, out_n);
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}
