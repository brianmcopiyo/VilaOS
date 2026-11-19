#include "vapi.h"
#include "bridge.h"
#include "contacts.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir_mode(p, m) _mkdir(p)
#else
#include <unistd.h>
#define mkdir_mode(p, m) mkdir(p, m)
#endif

#define DEFAULT_DIAL "555-0101"

static int g_muted;
static int g_speaker;

static void emit(VapiCtx *ctx, const char *widget, char *out, size_t out_n, const char *text)
{
    if (out && out_n)
        str_copy(out, out_n, text ? text : "");
    if (widget && widget[0] && ctx->rt)
        bridge_set_text(ctx->rt, widget, text ? text : "");
}

static void format_contact(const Contact *c, char *buf, size_t n)
{
    snprintf(buf, n, "%s \xC2\xB7 %s", c->name, c->number);
}

static int all_digits(const char *s)
{
    if (!s || !s[0])
        return 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9')
            return 0;
    }
    return 1;
}

static int start_call(VapiCtx *ctx, const char *a1, char *out, size_t out_n)
{
    char num[MAX_TEXT];
    char name[MAX_TEXT];
    num[0] = 0;
    if (a1 && a1[0])
        str_copy(num, sizeof(num), a1);
    else
        bridge_get_text(ctx->rt, "number", num, sizeof(num));
    if (!num[0])
        str_copy(num, sizeof(num), DEFAULT_DIAL);

    const Contact *c = contacts_by_number(num);
    str_copy(name, sizeof(name), c ? c->name : "Calling...");

    bridge_set_text(ctx->rt, "call_number", num);
    bridge_set_text(ctx->rt, "call_name", name);
    screen_carry_text("call_number", num);
    screen_carry_text("call_name", name);

    g_muted = 0;
    g_speaker = 0;
    str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "route:replace:res/layout/call_active.xml");
    if (out && out_n)
        str_copy(out, out_n, num);
    return VAPI_OK;
}

static int toggle_flag(VapiCtx *ctx, int *flag, const char *widget, const char *on_text, const char *off_text,
                       char *out, size_t out_n)
{
    *flag = !*flag;
    bridge_set_text(ctx->rt, widget, *flag ? on_text : off_text);
    if (out && out_n)
        str_copy(out, out_n, *flag ? "1" : "0");
    return VAPI_OK;
}

int vapi_telephony(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    if (!ctx || !ctx->rt || !op)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "digit")) {
        if (!a1 || !a1[0] || strlen(a1) > 4)
            return VAPI_ERR_INVALID_ARG;
        if (bridge_append_text(ctx->rt, "number", a1) != 0)
            return VAPI_ERR_NOT_FOUND;
        return VAPI_OK;
    }
    if (str_eq(op, "call") || str_eq(op, "start"))
        return start_call(ctx, a1, out, out_n);
    if (str_eq(op, "end") || str_eq(op, "hangup")) {
        g_muted = 0;
        g_speaker = 0;
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "route:pop");
        return VAPI_OK;
    }
    if (str_eq(op, "clear")) {
        if (bridge_clear_text(ctx->rt, "number") != 0)
            return VAPI_ERR_NOT_FOUND;
        return VAPI_OK;
    }
    if (str_eq(op, "mute"))
        return toggle_flag(ctx, &g_muted, "btn_mute", "Unmute", "Mute", out, out_n);
    if (str_eq(op, "speaker"))
        return toggle_flag(ctx, &g_speaker, "btn_speaker", "Earpiece", "Speaker", out, out_n);
    if (str_eq(op, "lookup")) {
        if (!a1 || !a1[0])
            return VAPI_ERR_INVALID_ARG;
        const Contact *c = contacts_by_number(a1);
        emit(ctx, a2, out, out_n, c ? c->name : "");
        return c ? VAPI_OK : VAPI_ERR_NOT_FOUND;
    }
    return VAPI_ERR_UNSUPPORTED;
}

static int outbox_append(const VapiCtx *ctx, const char *body)
{
    char dir[MAX_PATH];
    char path[MAX_PATH];
    if (!ctx->app || !ctx->app->id[0])
        return VAPI_ERR_INVALID_ARG;
    if (pkg_data_path(ctx->app->id, dir, sizeof(dir)) != 0)
        return VAPI_ERR_IO;
    if (snprintf(path, sizeof(path), "%s/outbox", dir) >= (int)sizeof(path))
        return VAPI_ERR_LIMIT;
    mkdir_mode(PKG_DATA_ROOT, 0755);
    mkdir_mode(dir, 0755);
    FILE *f = fopen(path, "a");
    if (!f)
        return VAPI_ERR_IO;
    fprintf(f, "%lld\t%s\n", (long long)time(NULL), body);
    fclose(f);
    return VAPI_OK;
}

static void message_body(VapiCtx *ctx, const char *a1, char *body, size_t n)
{
    static const char *const fallback[] = {"msg", "msg_input", "note", NULL};
    body[0] = 0;
    if (a1 && a1[0]) {
        if (bridge_get_text(ctx->rt, a1, body, n) != 0)
            str_copy(body, n, a1);
        return;
    }
    for (int i = 0; fallback[i] && !body[0]; i++)
        bridge_get_text(ctx->rt, fallback[i], body, n);
}

int vapi_messages(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
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
        char body[MAX_TEXT];
        message_body(ctx, a1, body, sizeof(body));
        if (!body[0]) {
            emit(ctx, a2, out, out_n, "Nothing to send");
            return VAPI_ERR_INVALID_ARG;
        }
        int st = outbox_append(ctx, body);
        if (st != VAPI_OK) {
            emit(ctx, a2, out, out_n, "Send failed");
            return st;
        }
        emit(ctx, a2, out, out_n, "Sent");
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}

int vapi_contacts(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    char line[MAX_TEXT];
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
        const Contact *c = contacts_search(a1);
        if (!c) {
            emit(ctx, a2, out, out_n, "No match");
            return VAPI_ERR_NOT_FOUND;
        }
        format_contact(c, line, sizeof(line));
        emit(ctx, a2, out, out_n, line);
        return VAPI_OK;
    }
    if (str_eq(op, "lookup") || str_eq(op, "get")) {
        if (!a1 || !a1[0])
            return VAPI_ERR_INVALID_ARG;
        const Contact *c = (all_digits(a1) && strlen(a1) <= 2) ? contacts_at(atoi(a1)) : contacts_by_number(a1);
        if (!c) {
            emit(ctx, a2, out, out_n, "");
            return VAPI_ERR_NOT_FOUND;
        }
        format_contact(c, line, sizeof(line));
        emit(ctx, a2, out, out_n, line);
        return VAPI_OK;
    }
    if (str_eq(op, "count")) {
        snprintf(line, sizeof(line), "%d", contacts_count());
        emit(ctx, a2, out, out_n, line);
        return VAPI_OK;
    }
    if (str_eq(op, "call")) {
        if (a1 && a1[0]) {
            bridge_set_text(ctx->rt, "number", a1);
            return start_call(ctx, a1, out, out_n);
        }
        str_copy(ctx->rt->nav, sizeof(ctx->rt->nav), "route:push:res/layout/detail.xml");
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
