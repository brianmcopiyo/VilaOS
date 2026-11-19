#include "vapi.h"
#include "bridge.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static void normalize_app_rel(const char *in, char *out, size_t n)
{
    if (!in || !out || n == 0) {
        if (out && n)
            out[0] = 0;
        return;
    }
    if (str_starts(in, "layout/")) {
        snprintf(out, n, "res/layout/%s", in + 7);
        return;
    }
    if (str_starts(in, "ui/")) {
        snprintf(out, n, "res/layout/%s", in + 3);
        return;
    }
    str_copy(out, n, in);
}

int vapi_ui(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    if (!ctx || !ctx->rt || !op)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "get_text") || str_eq(op, "text")) {
        if (!a1 || !a1[0] || !out || out_n == 0)
            return VAPI_ERR_INVALID_ARG;
        if (bridge_get_text(ctx->rt, a1, out, out_n) != 0)
            return VAPI_ERR_NOT_FOUND;
        return VAPI_OK;
    }

    if (str_eq(op, "push") || str_eq(op, "push_route")) {
        if (!a1 || !a1[0])
            return VAPI_ERR_INVALID_ARG;
        char rel[MAX_PATH];
        normalize_app_rel(a1, rel, sizeof(rel));
        snprintf(ctx->rt->nav, sizeof(ctx->rt->nav), "route:push:%s", rel);
        return VAPI_OK;
    }
    if (str_eq(op, "replace") || str_eq(op, "replace_route")) {
        if (!a1 || !a1[0])
            return VAPI_ERR_INVALID_ARG;
        char rel[MAX_PATH];
        normalize_app_rel(a1, rel, sizeof(rel));
        snprintf(ctx->rt->nav, sizeof(ctx->rt->nav), "route:replace:%s", rel);
        return VAPI_OK;
    }
    if (str_eq(op, "pop") || str_eq(op, "back") || str_eq(op, "pop_route")) {
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "route:pop");
        return VAPI_OK;
    }
    if (str_eq(op, "home") || str_eq(op, "go_home")) {
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "route:home");
        return VAPI_OK;
    }
    if (str_eq(op, "set_text")) {
        if (!a1 || !a1[0])
            return VAPI_ERR_INVALID_ARG;
        if (bridge_set_text(ctx->rt, a1, a2 ? a2 : "") != 0)
            return VAPI_ERR_NOT_FOUND;
        return VAPI_OK;
    }
    if (str_eq(op, "append_text")) {
        if (!a1 || !a1[0])
            return VAPI_ERR_INVALID_ARG;
        if (bridge_append_text(ctx->rt, a1, a2 ? a2 : "") != 0)
            return VAPI_ERR_NOT_FOUND;
        return VAPI_OK;
    }
    if (str_eq(op, "clear_text")) {
        if (!a1 || !a1[0])
            return VAPI_ERR_INVALID_ARG;
        if (bridge_clear_text(ctx->rt, a1) != 0)
            return VAPI_ERR_NOT_FOUND;
        return VAPI_OK;
    }
    if (str_eq(op, "softkey") || str_eq(op, "softkeys")) {
        if (!ctx->screen || !a1 || !a1[0])
            return VAPI_ERR_INVALID_ARG;
        if (str_eq(a1, "left"))
            str_copy(ctx->screen->soft_left, sizeof(ctx->screen->soft_left), a2 ? a2 : "");
        else if (str_eq(a1, "center"))
            str_copy(ctx->screen->soft_center, sizeof(ctx->screen->soft_center), a2 ? a2 : "");
        else if (str_eq(a1, "right"))
            str_copy(ctx->screen->soft_right, sizeof(ctx->screen->soft_right), a2 ? a2 : "");
        else if (str_eq(a1, "left_action"))
            str_copy(ctx->screen->soft_left_act, sizeof(ctx->screen->soft_left_act), a2 ? a2 : "");
        else if (str_eq(a1, "center_action"))
            str_copy(ctx->screen->soft_center_act, sizeof(ctx->screen->soft_center_act), a2 ? a2 : "");
        else if (str_eq(a1, "right_action"))
            str_copy(ctx->screen->soft_right_act, sizeof(ctx->screen->soft_right_act), a2 ? a2 : "");
        else
            return VAPI_ERR_INVALID_ARG;
        return VAPI_OK;
    }
    if (str_eq(op, "dialog") || str_eq(op, "alert")) {
        if (!a1)
            return VAPI_ERR_INVALID_ARG;
        Widget *title = screen_find(ctx->screen, "dialog_title");
        Widget *body = screen_find(ctx->screen, "dialog_body");
        if (title)
            str_copy(title->text, sizeof(title->text), a1);
        if (body)
            str_copy(body->text, sizeof(body->text), a2 ? a2 : "");
        if (!title && !body && ctx->screen) {
            str_copy(ctx->screen->soft_center, sizeof(ctx->screen->soft_center), a1);
            if (a2 && a2[0]) {
                Widget *hint = screen_find(ctx->screen, "hint");
                if (hint)
                    str_copy(hint->text, sizeof(hint->text), a2);
            }
        }
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}
