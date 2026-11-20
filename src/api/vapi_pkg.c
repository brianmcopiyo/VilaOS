#include "vapi.h"
#include "pkg.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

int vapi_pkg(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    if (!ctx || !op)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "refresh") || str_eq(op, "reload")) {
        int n = pkg_manager_refresh();
        if (out && out_n)
            snprintf(out, out_n, "%d", n);
        if (a1 && a1[0] && ctx->screen) {
            const char *shell = ctx->app && ctx->app->id[0] ? ctx->app->id : "launcher";
            pkg_manager_fill_grid(ctx->screen, a1, a2 ? a2 : "", shell);
        }
        return VAPI_OK;
    }
    if (str_eq(op, "count") || str_eq(op, "list.count")) {
        if (out && out_n)
            snprintf(out, out_n, "%d", pkg_manager_count());
        return VAPI_OK;
    }
    if (str_eq(op, "list") || str_eq(op, "listInstalled") || str_eq(op, "list_installed")) {
        if (out && out_n)
            pkg_manager_list_json(out, out_n);
        return VAPI_OK;
    }
    if (str_eq(op, "render") || str_eq(op, "fill") || str_eq(op, "renderGrid") || str_eq(op, "render_grid")) {
        if (!ctx->screen || !a1 || !a1[0])
            return VAPI_ERR_INVALID_ARG;
        const char *shell = ctx->app && ctx->app->id[0] ? ctx->app->id : "launcher";
        int n = pkg_manager_fill_grid(ctx->screen, a1, a2 ? a2 : "", shell);
        if (ctx->gfx)
            screen_layout(ctx->screen, ctx->gfx);
        if (out && out_n)
            snprintf(out, out_n, "%d", n);
        return VAPI_OK;
    }
    if (str_eq(op, "poll") || str_eq(op, "pollChanged") || str_eq(op, "poll_changed")) {
        if (out && out_n)
            snprintf(out, out_n, "%d", pkg_manager_poll_changed());
        return VAPI_OK;
    }
    if (str_eq(op, "get") || str_eq(op, "find")) {
        PkgApp app;
        if (!a1 || !a1[0])
            return VAPI_ERR_INVALID_ARG;
        if (pkg_manager_find(a1, &app) != 0)
            return VAPI_ERR_NOT_FOUND;
        if (out && out_n)
            snprintf(out, out_n, "%s:%s:%s:%s", app.id, app.name, app.version,
                     app.icon[0] ? app.icon : app.id);
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}
