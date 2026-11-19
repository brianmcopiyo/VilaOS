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

int vapi_system(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    (void)a2;
    if (!ctx || !op)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "display") || str_eq(op, "display.info") || str_eq(op, "device") ||
        str_eq(op, "device.info")) {
        char buf[MAX_TEXT];
        int w = 0, h = 0;
        if (ctx->gfx) {
            w = ctx->gfx->w;
            h = ctx->gfx->h;
        } else if (ctx->screen) {
            w = REF_W;
            h = REF_H;
        }
        HwProfile hw;
        hwinfo_detect(&hw);
        int aspect = 0;
        if (h > 0)
            aspect = (w * 100) / h;
        snprintf(buf, sizeof(buf), "%dx%d %d:%02d %s", w, h, aspect / 100, aspect % 100,
                 hwinfo_tier_name(hw.tier));
        if (str_eq(op, "device") || str_eq(op, "device.info")) {
            snprintf(buf, sizeof(buf), "%dx%d %s %ldMB %dc API%d", w, h, hwinfo_tier_name(hw.tier),
                     hw.ram_mb, hw.cores, VILAOS_API_VERSION);
        }
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
        snprintf(buf, sizeof(buf), "%ld/%ld MB %dc %s", avail_mb, total_mb, hw.cores,
                 hwinfo_tier_name(hw.tier));
        set_out(ctx, a1, buf, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "storage") || str_eq(op, "storage.info")) {
        char buf[MAX_TEXT];
        PkgRegistry reg;
        pkg_registry_load(&reg);
        if (reg.count == 0)
            pkg_seed_from_dir("/apps", &reg);
        snprintf(buf, sizeof(buf), "%d apps /packages /data/apps", reg.count);
        set_out(ctx, a1, buf, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "input") || str_eq(op, "input.info")) {
        const char *info = "Keypad + pointer";
        if (ctx->gfx && ctx->gfx->desktop)
            info = "Pointer + keypad";
        set_out(ctx, a1, info, out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "packages") || str_eq(op, "packages.list")) {
        char buf[MAX_TEXT];
        char countbuf[32];
        PkgRegistry reg;
        pkg_registry_load(&reg);
        if (reg.count == 0)
            pkg_seed_from_dir("/apps", &reg);
        if (pkg_list(&reg, buf, sizeof(buf)) < 0)
            str_copy(buf, sizeof(buf), "none");
        if (!buf[0])
            str_copy(buf, sizeof(buf), "none");
        snprintf(countbuf, sizeof(countbuf), "%d", reg.count);
        if (ctx->rt) {
            bridge_set_text(ctx->rt, "apps_count", countbuf);
            snprintf(countbuf, sizeof(countbuf), "%d packages", reg.count);
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
