#include "vapi.h"
#include "hwinfo.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static VapiLimits g_limits;
static int g_ready;

void vapi_init(void)
{
    HwProfile hw;
    hwinfo_detect(&hw);
    memset(&g_limits, 0, sizeof(g_limits));
    switch (hw.tier) {
    case HW_TIER_32G:
    case HW_TIER_8G:
        g_limits.arg_max = 4096;
        g_limits.path_max = 512;
        g_limits.buf_max = 65536;
        g_limits.queue_depth = 64;
        break;
    case HW_TIER_2G:
        g_limits.arg_max = 2048;
        g_limits.path_max = 384;
        g_limits.buf_max = 16384;
        g_limits.queue_depth = 32;
        break;
    case HW_TIER_512:
        g_limits.arg_max = 1024;
        g_limits.path_max = 256;
        g_limits.buf_max = 4096;
        g_limits.queue_depth = 16;
        break;
    case HW_TIER_256:
    default:
        g_limits.arg_max = 512;
        g_limits.path_max = 192;
        g_limits.buf_max = 1024;
        g_limits.queue_depth = 8;
        break;
    }
    g_limits.workers = hw.workers > 0 ? hw.workers : 1;
    g_ready = 1;
}

const VapiLimits *vapi_limits(void)
{
    if (!g_ready)
        vapi_init();
    return &g_limits;
}

const char *vapi_status_name(int st)
{
    switch (st) {
    case VAPI_OK:
        return "VAPI_OK";
    case VAPI_ERR_PERMISSION:
        return "VAPI_ERR_PERMISSION";
    case VAPI_ERR_INVALID_ARG:
        return "VAPI_ERR_INVALID_ARG";
    case VAPI_ERR_NOT_FOUND:
        return "VAPI_ERR_NOT_FOUND";
    case VAPI_ERR_LIMIT:
        return "VAPI_ERR_LIMIT";
    case VAPI_ERR_IO:
        return "VAPI_ERR_IO";
    case VAPI_ERR_UNSUPPORTED:
        return "VAPI_ERR_UNSUPPORTED";
    default:
        return "VAPI_ERR_INTERNAL";
    }
}

int vapi_check_cap(const VapiCtx *ctx, unsigned cap)
{
    if (!ctx || !ctx->app || cap == 0)
        return 0;
    return (ctx->app->caps & cap) == cap;
}

unsigned vapi_cap_required(const char *method)
{
    if (!method)
        return 0;
    if (str_starts(method, "ui."))
        return PKG_CAP_UI;
    if (str_starts(method, "window."))
        return PKG_CAP_UI;
    if (str_starts(method, "pkg."))
        return PKG_CAP_UI;
    if (str_starts(method, "storage.files."))
        return PKG_CAP_STORAGE_FILES;
    if (str_starts(method, "storage."))
        return PKG_CAP_STORAGE;
    if (str_starts(method, "service."))
        return PKG_CAP_AUDIO;
    if (str_starts(method, "audio."))
        return PKG_CAP_AUDIO;
    if (str_starts(method, "telephony."))
        return PKG_CAP_TELEPHONY;
    if (str_starts(method, "messages."))
        return PKG_CAP_MESSAGES;
    if (str_starts(method, "contacts."))
        return PKG_CAP_CONTACTS;
    if (str_starts(method, "clock."))
        return PKG_CAP_CLOCK;
    if (str_starts(method, "system.power"))
        return PKG_CAP_SYSTEM_POWER;
    if (str_starts(method, "system.clock"))
        return PKG_CAP_CLOCK;
    if (str_starts(method, "system.device") || str_starts(method, "system.display") ||
        str_starts(method, "system.graphics") || str_starts(method, "system.gpu") ||
        str_starts(method, "system.memory") || str_starts(method, "system.storage") ||
        str_starts(method, "system.input") || str_starts(method, "system.packages") ||
        str_starts(method, "system.runtime") || str_starts(method, "system.kernel") ||
        str_starts(method, "system.theme") || str_starts(method, "system.time") ||
        str_starts(method, "system.locale") || str_starts(method, "system.api"))
        return PKG_CAP_SYSTEM_DEVICE | PKG_CAP_DISPLAY;
    if (str_starts(method, "system."))
        return PKG_CAP_SYSTEM_DEVICE;
    return 0;
}

static int split_method(const char *method, char *domain, size_t dn, char *op, size_t on)
{
    if (!method || !domain || !op || dn == 0 || on == 0)
        return -1;
    domain[0] = 0;
    op[0] = 0;
    const char *dot = strchr(method, '.');
    if (!dot || dot == method || !dot[1])
        return -1;
    size_t dlen = (size_t)(dot - method);
    if (dlen >= dn)
        return -1;
    memcpy(domain, method, dlen);
    domain[dlen] = 0;
    str_copy(op, on, dot + 1);
    return 0;
}

static int validate_args(const VapiCtx *ctx, const char *a1, const char *a2)
{
    const VapiLimits *lim = ctx ? &ctx->limits : vapi_limits();
    if (a1 && strlen(a1) > lim->arg_max)
        return VAPI_ERR_LIMIT;
    if (a2 && strlen(a2) > lim->arg_max)
        return VAPI_ERR_LIMIT;
    return VAPI_OK;
}

int vapi_call(VapiCtx *ctx, const char *method, const char *a1, const char *a2, char *out, size_t out_n)
{
    if (out && out_n)
        out[0] = 0;
    if (!ctx || !method || !method[0])
        return VAPI_ERR_INVALID_ARG;
    if (!g_ready)
        vapi_init();
    ctx->limits = g_limits;

    int va = validate_args(ctx, a1, a2);
    if (va != VAPI_OK)
        return va;

    unsigned need = vapi_cap_required(method);
    if (need) {
        if (need == (PKG_CAP_SYSTEM_DEVICE | PKG_CAP_DISPLAY)) {
            if (!vapi_check_cap(ctx, PKG_CAP_SYSTEM_DEVICE) && !vapi_check_cap(ctx, PKG_CAP_DISPLAY))
                return VAPI_ERR_PERMISSION;
        } else if (!vapi_check_cap(ctx, need)) {
            return VAPI_ERR_PERMISSION;
        }
    }

    char domain[32];
    char op[64];
    if (split_method(method, domain, sizeof(domain), op, sizeof(op)) != 0)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(domain, "ui"))
        return vapi_ui(ctx, op, a1, a2, out, out_n);
    if (str_eq(domain, "storage"))
        return vapi_storage(ctx, op, a1, a2, out, out_n);
    if (str_eq(domain, "service"))
        return vapi_service(ctx, op, a1, a2, out, out_n);
    if (str_eq(domain, "audio"))
        return vapi_audio(ctx, op, a1, a2, out, out_n);
    if (str_eq(domain, "telephony"))
        return vapi_telephony(ctx, op, a1, a2, out, out_n);
    if (str_eq(domain, "messages"))
        return vapi_messages(ctx, op, a1, a2, out, out_n);
    if (str_eq(domain, "contacts"))
        return vapi_contacts(ctx, op, a1, a2, out, out_n);
    if (str_eq(domain, "clock"))
        return vapi_clock(ctx, op, a1, a2, out, out_n);
    if (str_eq(domain, "system"))
        return vapi_system(ctx, op, a1, a2, out, out_n);
    if (str_eq(domain, "window"))
        return vapi_window(ctx, op, a1, a2, out, out_n);
    if (str_eq(domain, "pkg"))
        return vapi_pkg(ctx, op, a1, a2, out, out_n);
    return VAPI_ERR_UNSUPPORTED;
}

int vapi_bridge_out(VslRuntime *rt, const char *method, const char *a1, const char *a2, char *out, size_t n)
{
    if (out && n)
        out[0] = 0;
    if (!rt || !method)
        return VAPI_ERR_INVALID_ARG;
    VapiCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.rt = rt;
    ctx.screen = rt->screen;
    ctx.app = &rt->app;
    ctx.gfx = NULL;
    char scratch[MAX_TEXT];
    if (!out || !n) {
        out = scratch;
        n = sizeof(scratch);
    }
    return vapi_call(&ctx, method, a1, a2, out, n);
}

int vapi_bridge(VslRuntime *rt, const char *method, const char *a1, const char *a2)
{
    if (!rt || !method)
        return -1;
    char out[MAX_TEXT];
    int st = vapi_bridge_out(rt, method, a1, a2, out, sizeof(out));
    return st == VAPI_OK ? 0 : -1;
}
