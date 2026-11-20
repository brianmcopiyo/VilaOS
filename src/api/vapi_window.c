#include "vapi.h"
#include "window.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

int vapi_window(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    (void)a2;
    if (!ctx || !op)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "open")) {
        if (!a1 || !a1[0] || !ctx->rt)
            return VAPI_ERR_INVALID_ARG;
        snprintf(ctx->rt->nav, sizeof(ctx->rt->nav), "app:%s", a1);
        return VAPI_OK;
    }
    if (str_eq(op, "drawer")) {
        if (!ctx->rt)
            return VAPI_ERR_INVALID_ARG;
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "route:push:res/layout/drawer.xml");
        return VAPI_OK;
    }
    if (str_eq(op, "show_desktop")) {
        if (!ctx->rt)
            return VAPI_ERR_INVALID_ARG;
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "wm:show_desktop");
        return VAPI_OK;
    }
    if (str_eq(op, "sync_taskbar")) {
        Screen *desk = g_wm.desktop ? g_wm.desktop : ctx->screen;
        if (desk)
            wm_sync_taskbar(&g_wm, desk);
        return VAPI_OK;
    }
    if (str_eq(op, "count")) {
        int n = 0;
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (g_wm.wins[i].alive)
                n++;
        }
        if (out && out_n)
            snprintf(out, out_n, "%d", n);
        return VAPI_OK;
    }
    if (str_eq(op, "focused")) {
        OsWindow *w = wm_focused(&g_wm);
        const char *title = w ? w->title : "None";
        if (out && out_n)
            str_copy(out, out_n, title && title[0] ? title : "None");
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}
