#include "vsl.h"
#include "bridge.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void vsl_reset(VslRuntime *rt)
{
    if (!rt)
        return;
    memset(rt, 0, sizeof(*rt));
}

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    return s;
}

static void unquote(char *s)
{
    size_t n = strlen(s);
    if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') || (s[0] == '\'' && s[n - 1] == '\''))) {
        s[n - 1] = 0;
        memmove(s, s + 1, n - 1);
    }
}

static VslVar *var_get(VslRuntime *rt, const char *name, int create)
{
    for (int i = 0; i < rt->var_count; i++) {
        if (str_eq(rt->vars[i].name, name))
            return &rt->vars[i];
    }
    if (!create || rt->var_count >= VSL_MAX_VARS)
        return NULL;
    VslVar *v = &rt->vars[rt->var_count++];
    memset(v, 0, sizeof(*v));
    str_copy(v->name, sizeof(v->name), name);
    return v;
}

static void resolve_token(VslRuntime *rt, const char *tok, char *out, size_t n)
{
    out[0] = 0;
    if (!tok || !tok[0])
        return;
    if (tok[0] == '$') {
        VslVar *v = var_get(rt, tok + 1, 0);
        if (v)
            str_copy(out, n, v->value);
        return;
    }
    if (str_starts(tok, "text:")) {
        bridge_get_text(rt, tok + 5, out, n);
        return;
    }
    str_copy(out, n, tok);
    unquote(out);
}

static int exec_line(VslRuntime *rt, char *line)
{
    line = trim(line);
    if (!line[0] || line[0] == '#')
        return 0;
    char buf[256];
    str_copy(buf, sizeof(buf), line);
    char *cmd = strtok(buf, " \t");
    if (!cmd)
        return 0;
    char *a1 = strtok(NULL, " \t");
    char *a2 = strtok(NULL, "");
    if (a1)
        a1 = trim(a1);
    if (a2)
        a2 = trim(a2);
    if (a1)
        unquote(a1);
    if (a2)
        unquote(a2);

    if (str_eq(cmd, "let") && a1 && a2) {
        char val[MAX_TEXT];
        resolve_token(rt, a2, val, sizeof(val));
        VslVar *v = var_get(rt, a1, 1);
        if (v)
            str_copy(v->value, sizeof(v->value), val);
        return 0;
    }
    if (strchr(cmd, '.')) {
        char val1[MAX_TEXT];
        char val2[MAX_TEXT];
        const char *arg1 = a1;
        const char *arg2 = a2;
        if (a1) {
            resolve_token(rt, a1, val1, sizeof(val1));
            arg1 = val1;
        }
        if (a2) {
            resolve_token(rt, a2, val2, sizeof(val2));
            arg2 = val2;
        }
        return bridge_exec_cmd(rt, cmd, arg1, arg2);
    }
    if (str_eq(cmd, "set_text") && a1 && a2) {
        char val[MAX_TEXT];
        resolve_token(rt, a2, val, sizeof(val));
        return bridge_exec_cmd(rt, "ui.set_text", a1, val);
    }
    if (str_eq(cmd, "append_text") && a1 && a2) {
        char val[MAX_TEXT];
        resolve_token(rt, a2, val, sizeof(val));
        return bridge_exec_cmd(rt, "ui.append_text", a1, val);
    }
    if (str_eq(cmd, "clear_text") && a1)
        return bridge_exec_cmd(rt, "ui.clear_text", a1, NULL);
    if (str_eq(cmd, "nav") && a1)
        return bridge_nav(rt, a1);
    if (str_eq(cmd, "push_route") && a1)
        return bridge_exec_cmd(rt, "ui.push", a1, NULL);
    if (str_eq(cmd, "replace_route") && a1)
        return bridge_exec_cmd(rt, "ui.replace", a1, NULL);
    if (str_eq(cmd, "pop_route") || str_eq(cmd, "back"))
        return bridge_exec_cmd(rt, "ui.pop", NULL, NULL);
    if (str_eq(cmd, "go_home") || str_eq(cmd, "home"))
        return bridge_exec_cmd(rt, "ui.home", NULL, NULL);
    if (str_eq(cmd, "call"))
        return bridge_exec_cmd(rt, "telephony.call", NULL, NULL);
    if (str_eq(cmd, "require") && a1)
        return bridge_exec_cmd(rt, "require", a1, NULL);
    if (str_eq(cmd, "storage_set") && a1 && a2) {
        char val[MAX_TEXT];
        resolve_token(rt, a2, val, sizeof(val));
        return bridge_exec_cmd(rt, "storage.app.set", a1, val);
    }
    if (str_eq(cmd, "storage_get") && a1 && a2)
        return bridge_exec_cmd(rt, "storage.app.get", a1, a2);
    if (str_eq(cmd, "digit") && a1)
        return bridge_exec_cmd(rt, "telephony.digit", a1, NULL);
    if (str_eq(cmd, "action") && a1) {
        char nav[MAX_PATH];
        nav[0] = 0;
        int rc = screen_handle_action(rt->screen, a1, nav, sizeof(nav));
        if (rc == 1 && nav[0])
            str_copy(rt->nav, sizeof(rt->nav), nav);
        return rc;
    }
    return bridge_exec_cmd(rt, cmd, a1, a2);
}

static int run_range(VslRuntime *rt, int start, int end)
{
    for (int i = start; i <= end && i < rt->line_count; i++) {
        char line[256];
        str_copy(line, sizeof(line), rt->lines[i] ? rt->lines[i] : "");
        char *t = trim(line);
        if (str_eq(t, "end") || str_starts(t, "on_"))
            break;
        if (exec_line(rt, t) < 0 && str_starts(t, "require"))
            return -1;
    }
    return 0;
}

static int ends_with(const char *s, const char *suf)
{
    size_t n, m;
    if (!s || !suf)
        return 0;
    n = strlen(s);
    m = strlen(suf);
    if (m > n)
        return 0;
    return str_eq(s + (n - m), suf);
}

int vsl_load(VslRuntime *rt, const PkgApp *app, Screen *screen, const char *script_path)
{
    char resolved[MAX_PATH];
    if (!rt || !app || !screen)
        return -1;
    vsl_reset(rt);
    rt->app = *app;
    rt->screen = screen;
    if (!script_path || !script_path[0]) {
        rt->loaded = 1;
        return 0;
    }
    str_copy(resolved, sizeof(resolved), script_path);
    if (ends_with(resolved, ".kt")) {
        char alt[MAX_PATH];
        str_copy(alt, sizeof(alt), resolved);
        char *slash = strrchr(alt, '/');
        if (slash) {
            slash[1] = 0;
            str_cat(alt, sizeof(alt), "../bin/main.kbc");
        } else {
            str_copy(alt, sizeof(alt), "bin/main.kbc");
        }
        FILE *altf = fopen(alt, "r");
        if (altf) {
            fclose(altf);
            str_copy(resolved, sizeof(resolved), alt);
        }
    }
    FILE *f = fopen(resolved, "r");
    if (!f) {
        rt->loaded = 1;
        return 0;
    }
    size_t n = fread(rt->src, 1, sizeof(rt->src) - 1, f);
    fclose(f);
    rt->src[n] = 0;

    char *p = rt->src;
    while (*p && rt->line_count < VSL_MAX_LINES) {
        rt->lines[rt->line_count++] = p;
        char *nl = strchr(p, '\n');
        if (!nl)
            break;
        *nl = 0;
        if (nl > p && nl[-1] == '\r')
            nl[-1] = 0;
        p = nl + 1;
    }

    for (int i = 0; i < rt->line_count; i++) {
        char line[256];
        str_copy(line, sizeof(line), rt->lines[i] ? rt->lines[i] : "");
        char *t = trim(line);
        if (!str_starts(t, "on_"))
            continue;
        if (rt->handler_count >= VSL_MAX_HANDLERS)
            break;
        VslHandler *h = &rt->handlers[rt->handler_count++];
        memset(h, 0, sizeof(*h));
        char ev[24] = {0};
        char tgt[MAX_ID] = {0};
        if (sscanf(t, "on_%23s %31s", ev, tgt) >= 1) {
            str_copy(h->event, sizeof(h->event), ev);
            if (tgt[0]) {
                unquote(tgt);
                str_copy(h->target, sizeof(h->target), tgt);
            }
        }
        h->line_start = i + 1;
        h->line_end = rt->line_count - 1;
        for (int j = i + 1; j < rt->line_count; j++) {
            char l2[256];
            str_copy(l2, sizeof(l2), rt->lines[j] ? rt->lines[j] : "");
            char *t2 = trim(l2);
            if (str_eq(t2, "end") || str_starts(t2, "on_")) {
                h->line_end = j - 1;
                break;
            }
        }
    }
    rt->loaded = 1;
    vsl_fire(rt, "load", "");
    return 0;
}

int vsl_fire(VslRuntime *rt, const char *event, const char *target)
{
    if (!rt || !rt->loaded || !event)
        return 0;
    if (target) {
        VslVar *v = var_get(rt, "key", 1);
        if (v)
            str_copy(v->value, sizeof(v->value), target);
        v = var_get(rt, "target", 1);
        if (v)
            str_copy(v->value, sizeof(v->value), target);
        v = var_get(rt, "event", 1);
        if (v)
            str_copy(v->value, sizeof(v->value), event);
    }
    int matched = 0;
    for (int i = 0; i < rt->handler_count; i++) {
        VslHandler *h = &rt->handlers[i];
        if (!str_eq(h->event, event))
            continue;
        if (h->target[0] && target && target[0] && !str_eq(h->target, target) && !str_eq(h->target, "*"))
            continue;
        if (h->target[0] && (!target || !target[0]) && !str_eq(h->target, "*"))
            continue;
        run_range(rt, h->line_start, h->line_end);
        matched = 1;
    }
    return matched;
}

int vsl_has_nav(const VslRuntime *rt)
{
    return rt && rt->nav[0] != 0;
}

const char *vsl_take_nav(VslRuntime *rt)
{
    if (!rt || !rt->nav[0])
        return "";
    return rt->nav;
}

int vsl_consume_nav(VslRuntime *rt, char *out, size_t out_sz)
{
    if (!out || out_sz == 0)
        return 0;
    out[0] = 0;
    if (!rt || !rt->nav[0])
        return 0;
    str_copy(out, out_sz, rt->nav);
    rt->nav[0] = 0;
    return 1;
}
