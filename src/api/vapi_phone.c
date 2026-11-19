#include "vapi.h"
#include "bridge.h"
#include "util.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

int vapi_telephony(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    (void)a2;
    (void)out;
    (void)out_n;
    if (!ctx || !ctx->rt || !op)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "digit")) {
        if (!a1 || !a1[0] || strlen(a1) > 4)
            return VAPI_ERR_INVALID_ARG;
        if (bridge_append_text(ctx->rt, "number", a1) != 0)
            return VAPI_ERR_NOT_FOUND;
        return VAPI_OK;
    }
    if (str_eq(op, "call") || str_eq(op, "start")) {
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "route:replace:res/layout/call_active.xml");
        return VAPI_OK;
    }
    if (str_eq(op, "end") || str_eq(op, "hangup")) {
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "route:pop");
        return VAPI_OK;
    }
    if (str_eq(op, "clear")) {
        if (bridge_clear_text(ctx->rt, "number") != 0)
            return VAPI_ERR_NOT_FOUND;
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}

int vapi_messages(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    (void)a2;
    (void)out;
    (void)out_n;
    if (!ctx || !ctx->rt || !op)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "open") || str_eq(op, "thread") || str_eq(op, "open_thread")) {
        const char *layout = (a1 && a1[0]) ? a1 : "res/layout/thread.xml";
        snprintf(ctx->rt->nav, sizeof(ctx->rt->nav), "route:push:%s", layout);
        return VAPI_OK;
    }
    if (str_eq(op, "compose") || str_eq(op, "new")) {
        const char *layout = (a1 && a1[0]) ? a1 : "res/layout/compose.xml";
        snprintf(ctx->rt->nav, sizeof(ctx->rt->nav), "route:push:%s", layout);
        return VAPI_OK;
    }
    if (str_eq(op, "send")) {
        if (a1 && a1[0] && ctx->screen) {
            Widget *status = screen_find(ctx->screen, a1);
            if (status)
                str_copy(status->text, sizeof(status->text), "Sent");
        }
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}

int vapi_contacts(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    (void)a2;
    (void)out;
    (void)out_n;
    if (!ctx || !ctx->rt || !op)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "open") || str_eq(op, "detail") || str_eq(op, "view")) {
        const char *layout = (a1 && a1[0]) ? a1 : "res/layout/detail.xml";
        snprintf(ctx->rt->nav, sizeof(ctx->rt->nav), "route:push:%s", layout);
        return VAPI_OK;
    }
    if (str_eq(op, "search")) {
        if (!a1 || !a1[0])
            return VAPI_ERR_INVALID_ARG;
        Widget *match = screen_find(ctx->screen, a2 && a2[0] ? a2 : "contact_match");
        if (match)
            str_copy(match->text, sizeof(match->text), a1);
        return VAPI_OK;
    }
    if (str_eq(op, "call")) {
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "route:replace:res/layout/call_active.xml");
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}

int vapi_clock(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    (void)a2;
    if (!ctx || !op)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "now") || str_eq(op, "time") || str_eq(op, "get")) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char buf[64];
        if (!tm)
            return VAPI_ERR_IO;
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
        if (a1 && a1[0] && ctx->rt)
            bridge_set_text(ctx->rt, a1, buf);
        if (out && out_n)
            str_copy(out, out_n, buf);
        return VAPI_OK;
    }
    if (str_eq(op, "date")) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char buf[64];
        if (!tm)
            return VAPI_ERR_IO;
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
        if (a1 && a1[0] && ctx->rt)
            bridge_set_text(ctx->rt, a1, buf);
        if (out && out_n)
            str_copy(out, out_n, buf);
        return VAPI_OK;
    }
    if (str_eq(op, "tick") || str_eq(op, "sync"))
        return VAPI_OK;
    return VAPI_ERR_UNSUPPORTED;
}
