#include "bridge.h"
#include "vapi.h"
#include "procman.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir_mode(p, m) _mkdir(p)
#else
#include <unistd.h>
#define mkdir_mode(p, m) mkdir(p, m)
#endif

int bridge_check_cap(const VslRuntime *rt, unsigned cap)
{
    if (!rt || cap == 0)
        return 0;
    return (rt->app.caps & cap) == cap;
}

int bridge_set_text(VslRuntime *rt, const char *id, const char *text)
{
    if (!rt || !rt->screen || !id)
        return -1;
    Widget *w = screen_find(rt->screen, id);
    if (!w)
        return -1;
    str_copy(w->text, sizeof(w->text), text ? text : "");
    return 0;
}

int bridge_get_text(VslRuntime *rt, const char *id, char *out, size_t n)
{
    if (!rt || !rt->screen || !id || !out)
        return -1;
    Widget *w = screen_find(rt->screen, id);
    if (!w) {
        out[0] = 0;
        return -1;
    }
    str_copy(out, n, w->text);
    return 0;
}

int bridge_append_text(VslRuntime *rt, const char *id, const char *text)
{
    char cur[MAX_TEXT];
    if (bridge_get_text(rt, id, cur, sizeof(cur)) != 0)
        cur[0] = 0;
    str_cat(cur, sizeof(cur), text ? text : "");
    return bridge_set_text(rt, id, cur);
}

int bridge_clear_text(VslRuntime *rt, const char *id)
{
    return bridge_set_text(rt, id, "");
}

static int ensure_data_dir(const VslRuntime *rt)
{
    char path[MAX_PATH];
    if (!bridge_check_cap(rt, PKG_CAP_STORAGE))
        return -1;
    pkg_data_path(rt->app.id, path, sizeof(path));
    mkdir_mode(PKG_DATA_ROOT, 0755);
    mkdir_mode(path, 0755);
    return 0;
}

int bridge_storage_set(VslRuntime *rt, const char *key, const char *val)
{
    return vapi_bridge(rt, "storage.app.set", key, val);
}

int bridge_storage_get(VslRuntime *rt, const char *key, char *out, size_t n)
{
    if (!rt || !key || !out || n == 0)
        return -1;
    out[0] = 0;
    VapiCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.rt = rt;
    ctx.screen = rt->screen;
    ctx.app = &rt->app;
    return vapi_call(&ctx, "storage.app.get", key, NULL, out, n) == VAPI_OK ? 0 : -1;
}

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
    if (str_starts(in, "logic/")) {
        snprintf(out, n, "src/%s", in + 6);
        return;
    }
    if (str_starts(in, "assets/") && !str_starts(in, "assets/screens/")) {
        snprintf(out, n, "res/assets/%s", in + 7);
        return;
    }
    str_copy(out, n, in);
}

int bridge_nav(VslRuntime *rt, const char *screen_rel)
{
    if (!rt || !screen_rel)
        return -1;
    if (str_starts(screen_rel, "route:") || str_starts(screen_rel, "app:") ||
        str_starts(screen_rel, "assets/screens/")) {
        str_copy(rt->nav, sizeof(rt->nav), screen_rel);
        return 0;
    }
    char rel[MAX_PATH];
    normalize_app_rel(screen_rel, rel, sizeof(rel));
    snprintf(rt->nav, sizeof(rt->nav), "route:push:%s", rel);
    return 0;
}

int bridge_exec_cmd(VslRuntime *rt, const char *cmd, const char *a1, const char *a2)
{
    if (!rt || !cmd)
        return -1;

    if (strchr(cmd, '.'))
        return vapi_bridge(rt, cmd, a1, a2);

    if (str_eq(cmd, "set_text"))
        return vapi_bridge(rt, "ui.set_text", a1, a2);
    if (str_eq(cmd, "append_text"))
        return vapi_bridge(rt, "ui.append_text", a1, a2);
    if (str_eq(cmd, "clear_text"))
        return vapi_bridge(rt, "ui.clear_text", a1, NULL);
    if (str_eq(cmd, "nav"))
        return bridge_nav(rt, a1);
    if (str_eq(cmd, "back") || str_eq(cmd, "pop_route"))
        return vapi_bridge(rt, "ui.pop", NULL, NULL);
    if (str_eq(cmd, "home") || str_eq(cmd, "go_home"))
        return vapi_bridge(rt, "ui.home", NULL, NULL);
    if (str_eq(cmd, "push_route") && a1)
        return vapi_bridge(rt, "ui.push", a1, NULL);
    if (str_eq(cmd, "replace_route") && a1)
        return vapi_bridge(rt, "ui.replace", a1, NULL);
    if (str_eq(cmd, "require")) {
        unsigned c = pkg_cap_from_name(a1);
        if (!c)
            return -1;
        return bridge_check_cap(rt, c) ? 0 : -1;
    }
    if (str_eq(cmd, "storage_set"))
        return vapi_bridge(rt, "storage.app.set", a1, a2);
    if (str_eq(cmd, "storage_get"))
        return vapi_bridge(rt, "storage.app.get", a1, a2);
    if (str_eq(cmd, "call"))
        return vapi_bridge(rt, "telephony.call", NULL, NULL);
    if (str_eq(cmd, "digit") && a1)
        return vapi_bridge(rt, "telephony.digit", a1, NULL);
    if (str_eq(cmd, "svc_start") && a1)
        return vapi_bridge(rt, "service.start", a1, NULL);
    if (str_eq(cmd, "svc_stop") && a1)
        return vapi_bridge(rt, "service.stop", a1, NULL);
    if (str_eq(cmd, "svc_status") && a1)
        return vapi_bridge(rt, "service.status", a1, a2);
    if (str_eq(cmd, "svc_cmd") && a1 && a2)
        return vapi_bridge(rt, "service.cmd", a1, a2);
    (void)ensure_data_dir;
    return -1;
}
