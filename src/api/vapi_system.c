#include "vapi.h"
#include "bridge.h"
#include "hwinfo.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

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
        snprintf(buf, sizeof(buf), "%dx%d %s %ldMB API%d", w, h, hwinfo_tier_name(hw.tier), hw.ram_mb,
                 VILAOS_API_VERSION);
        if (a1 && a1[0] && ctx->rt)
            bridge_set_text(ctx->rt, a1, buf);
        if (out && out_n)
            str_copy(out, out_n, buf);
        return VAPI_OK;
    }
    if (str_eq(op, "time") || str_eq(op, "clock") || str_eq(op, "clock.now")) {
        return vapi_clock(ctx, "now", a1, NULL, out, out_n);
    }
    if (str_eq(op, "locale") || str_eq(op, "locale.get")) {
        const char *loc = "en-US";
        if (a1 && a1[0] && ctx->rt)
            bridge_set_text(ctx->rt, a1, loc);
        if (out && out_n)
            str_copy(out, out_n, loc);
        return VAPI_OK;
    }
    if (str_eq(op, "theme") || str_eq(op, "theme.get")) {
        const char *theme = "default";
        if (a1 && a1[0] && ctx->rt)
            bridge_set_text(ctx->rt, a1, theme);
        if (out && out_n)
            str_copy(out, out_n, theme);
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
        if (a1 && a1[0] && ctx->rt)
            bridge_set_text(ctx->rt, a1, buf);
        else if (out && out_n)
            str_copy(out, out_n, buf);
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}
