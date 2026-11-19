#include "vapi.h"
#include "bridge.h"
#include "procman.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

int vapi_service(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    if (!ctx || !op)
        return VAPI_ERR_INVALID_ARG;
    if (!a1 || !a1[0] || strlen(a1) >= PROCMAN_ID_LEN)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "start")) {
        if (procman_start(&g_procman, a1) != 0)
            return VAPI_ERR_IO;
        return VAPI_OK;
    }
    if (str_eq(op, "stop")) {
        procman_stop(&g_procman, a1);
        return VAPI_OK;
    }
    if (str_eq(op, "status") || str_eq(op, "query")) {
        char st[MAX_TEXT];
        if (procman_status(&g_procman, a1, st, sizeof(st)) != 0)
            return VAPI_ERR_NOT_FOUND;
        if (a2 && a2[0] && ctx->rt) {
            if (bridge_set_text(ctx->rt, a2, st) != 0)
                return VAPI_ERR_NOT_FOUND;
        } else if (out && out_n) {
            str_copy(out, out_n, st);
        }
        return VAPI_OK;
    }
    if (str_eq(op, "cmd") || str_eq(op, "send")) {
        if (!a2 || !a2[0])
            return VAPI_ERR_INVALID_ARG;
        char cmdbuf[128];
        char label[MAX_ID];
        char resp[MAX_TEXT];
        label[0] = 0;
        str_copy(cmdbuf, sizeof(cmdbuf), a2);
        char *sp = strchr(cmdbuf, ' ');
        if (sp) {
            *sp = 0;
            sp++;
            while (*sp == ' ')
                sp++;
            if (*sp)
                str_copy(label, sizeof(label), sp);
        }
        if (strlen(cmdbuf) > ctx->limits.arg_max)
            return VAPI_ERR_LIMIT;
        if (procman_cmd(&g_procman, a1, cmdbuf, resp, sizeof(resp)) != 0)
            return VAPI_ERR_IO;
        if (label[0] && ctx->rt)
            bridge_set_text(ctx->rt, label, resp);
        else if (out && out_n)
            str_copy(out, out_n, resp);
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}

int vapi_audio(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    const char *svc = "musicd";
    const char *label = "now_playing";
    char combined[160];

    if (!ctx || !op)
        return VAPI_ERR_INVALID_ARG;
    if (a1 && a1[0] && str_eq(a1, "musicd"))
        svc = a1;
    if (a1 && a1[0] && !str_eq(a1, "musicd"))
        label = a1;
    if (a2 && a2[0])
        label = a2;

    if (str_eq(op, "play")) {
        if (procman_start(&g_procman, svc) != 0)
            return VAPI_ERR_IO;
        snprintf(combined, sizeof(combined), "PLAY %s", label);
        return vapi_service(ctx, "cmd", svc, combined, out, out_n);
    }
    if (str_eq(op, "pause")) {
        snprintf(combined, sizeof(combined), "PAUSE %s", label);
        return vapi_service(ctx, "cmd", svc, combined, out, out_n);
    }
    if (str_eq(op, "skip")) {
        snprintf(combined, sizeof(combined), "SKIP %s", label);
        return vapi_service(ctx, "cmd", svc, combined, out, out_n);
    }
    if (str_eq(op, "stop")) {
        procman_stop(&g_procman, svc);
        if (ctx->rt) {
            bridge_set_text(ctx->rt, label, "Stopped");
            if (a2 && a2[0])
                bridge_set_text(ctx->rt, a2, "service: stopped");
            else if (a1 && a1[0] && !str_eq(a1, label))
                bridge_set_text(ctx->rt, a1, "service: stopped");
        }
        return VAPI_OK;
    }
    if (str_eq(op, "status")) {
        char st[MAX_TEXT];
        char track[MAX_TEXT];
        procman_status(&g_procman, svc, st, sizeof(st));
        if (procman_cmd(&g_procman, svc, "STATUS", track, sizeof(track)) != 0)
            str_copy(track, sizeof(track), "");
        if (ctx->rt)
            bridge_set_text(ctx->rt, label, track[0] ? track : st);
        if (a2 && a2[0] && ctx->rt && !str_eq(a2, label))
            bridge_set_text(ctx->rt, a2, st);
        if (out && out_n)
            str_copy(out, out_n, track[0] ? track : st);
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}
