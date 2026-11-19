#include "bridge.h"
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
    if (!rt)
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
    if (!rt || !key)
        return -1;
    if (ensure_data_dir(rt) != 0)
        return -1;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s/%s", PKG_DATA_ROOT, rt->app.id, key);
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fputs(val ? val : "", f);
    fclose(f);
    return 0;
}

int bridge_storage_get(VslRuntime *rt, const char *key, char *out, size_t n)
{
    if (!rt || !key || !out || n == 0)
        return -1;
    out[0] = 0;
    if (!bridge_check_cap(rt, PKG_CAP_STORAGE))
        return -1;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s/%s", PKG_DATA_ROOT, rt->app.id, key);
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    if (!fgets(out, (int)n, f))
        out[0] = 0;
    fclose(f);
    size_t L = strlen(out);
    while (L > 0 && (out[L - 1] == '\n' || out[L - 1] == '\r'))
        out[--L] = 0;
    return 0;
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
    if (str_eq(cmd, "set_text"))
        return bridge_set_text(rt, a1, a2);
    if (str_eq(cmd, "append_text"))
        return bridge_append_text(rt, a1, a2);
    if (str_eq(cmd, "clear_text"))
        return bridge_clear_text(rt, a1);
    if (str_eq(cmd, "nav"))
        return bridge_nav(rt, a1);
    if (str_eq(cmd, "back") || str_eq(cmd, "pop_route")) {
        str_copy(rt->nav, sizeof(rt->nav), "route:pop");
        return 0;
    }
    if (str_eq(cmd, "home") || str_eq(cmd, "go_home")) {
        str_copy(rt->nav, sizeof(rt->nav), "route:home");
        return 0;
    }
    if (str_eq(cmd, "push_route") && a1) {
        char rel[MAX_PATH];
        normalize_app_rel(a1, rel, sizeof(rel));
        snprintf(rt->nav, sizeof(rt->nav), "route:push:%s", rel);
        return 0;
    }
    if (str_eq(cmd, "replace_route") && a1) {
        char rel[MAX_PATH];
        normalize_app_rel(a1, rel, sizeof(rel));
        snprintf(rt->nav, sizeof(rt->nav), "route:replace:%s", rel);
        return 0;
    }
    if (str_eq(cmd, "require")) {
        unsigned c = pkg_cap_from_name(a1);
        return bridge_check_cap(rt, c) ? 0 : -1;
    }
    if (str_eq(cmd, "storage_set"))
        return bridge_storage_set(rt, a1, a2);
    if (str_eq(cmd, "storage_get")) {
        char val[MAX_TEXT];
        if (bridge_storage_get(rt, a1, val, sizeof(val)) != 0)
            val[0] = 0;
        return bridge_set_text(rt, a2, val);
    }
    if (str_eq(cmd, "call")) {
        if (!bridge_check_cap(rt, PKG_CAP_TELEPHONY))
            return -1;
        str_copy(rt->nav, sizeof(rt->nav), "route:replace:res/layout/call_active.xml");
        return 0;
    }
    return -1;
}
