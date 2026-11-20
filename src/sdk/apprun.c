#include "apprun.h"
#include "bridge.h"
#include "util.h"
#include "window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int resolve_app(AppHost *h, const char *id, PkgApp *app);

static void sync_host_from_window(AppHost *h, OsWindow *win)
{
    if (!h || !win)
        return;
    h->current = win->app;
    h->in_app = 1;
    h->stack = win->stack;
    h->vsl.screen = &win->screen;
}

static void save_host_to_window(AppHost *h, OsWindow *win)
{
    if (!h || !win)
        return;
    win->stack = h->stack;
    win->app = h->current;
}

static void bind_shell_host(AppHost *h, Screen *desktop)
{
    PkgApp shell;
    if (!h)
        return;
    resolve_app(h, h->shell_app[0] ? h->shell_app : "launcher", &shell);
    h->current = shell;
    h->in_app = 1;
    if (desktop)
        h->vsl.screen = desktop;
}

static int is_shell_app(const AppHost *h, const char *id)
{
    if (!h || !id || !id[0])
        return 0;
    if (h->shell_app[0] && str_eq(id, h->shell_app))
        return 1;
    return str_eq(id, "launcher");
}

void apphost_push_recent(AppHost *h, const char *id)
{
    if (!h || !id || !id[0] || is_shell_app(h, id))
        return;
    for (int i = 0; i < h->recent_count; i++) {
        if (str_eq(h->recents[i], id)) {
            char tmp[MAX_ID];
            str_copy(tmp, sizeof(tmp), h->recents[i]);
            for (int j = i; j > 0; j--)
                str_copy(h->recents[j], sizeof(h->recents[j]), h->recents[j - 1]);
            str_copy(h->recents[0], sizeof(h->recents[0]), tmp);
            return;
        }
    }
    if (h->recent_count < APPHOST_MAX_RECENTS)
        h->recent_count++;
    for (int i = h->recent_count - 1; i > 0; i--)
        str_copy(h->recents[i], sizeof(h->recents[i]), h->recents[i - 1]);
    str_copy(h->recents[0], sizeof(h->recents[0]), id);
}

void apphost_clear_recents(AppHost *h)
{
    if (!h)
        return;
    h->recent_count = 0;
    memset(h->recents, 0, sizeof(h->recents));
}

int apphost_refresh_launcher(AppHost *h, Screen *s)
{
    if (!h || !s)
        return -1;
    pkg_manager_refresh();
    {
        const PkgRegistry *reg = pkg_manager_registry();
        if (reg)
            h->reg = *reg;
    }
    const char *shell = h->shell_app[0] ? h->shell_app : "launcher";
    pkg_manager_fill_grid(s, "DynamicAppGrid", "", shell);
    pkg_manager_fill_grid(s, "launcher", "", shell);
    pkg_manager_fill_grid(s, "drawer", "", shell);
    return 0;
}

int apphost_refresh_recents(AppHost *h, Screen *s)
{
    if (!h || !s)
        return -1;
    for (int i = 0; i < APPHOST_MAX_RECENTS; i++) {
        char wid[8];
        snprintf(wid, sizeof(wid), "r%d", i);
        Widget *row = screen_find(s, wid);
        if (!row)
            continue;
        if (i < h->recent_count) {
            int idx = pkg_registry_find(&h->reg, h->recents[i]);
            const char *name = h->recents[i];
            const char *icon = h->recents[i];
            if (idx >= 0) {
                name = h->reg.apps[idx].name;
                if (h->reg.apps[idx].icon[0])
                    icon = h->reg.apps[idx].icon;
            }
            snprintf(row->text, sizeof(row->text), "%s\nRunning", name);
            str_copy(row->icon, sizeof(row->icon), icon);
            snprintf(row->action, sizeof(row->action), "app:%s", h->recents[i]);
            row->visible = 1;
            row->focusable = 1;
        } else {
            str_copy(row->text, sizeof(row->text), "—\nNot running");
            str_copy(row->icon, sizeof(row->icon), "app");
            str_copy(row->action, sizeof(row->action), "none");
            row->visible = 1;
            row->focusable = 0;
        }
    }
    return 0;
}

int apphost_init(AppHost *h)
{
    if (!h)
        return -1;
    memset(h, 0, sizeof(*h));
    str_copy(h->shell_app, sizeof(h->shell_app), "launcher");
    str_copy(h->shell_path, sizeof(h->shell_path), "app:launcher");
    h->unlocked = 1;
    route_reset(&h->stack);
    pkg_manager_init();
    {
        const PkgRegistry *reg = pkg_manager_registry();
        if (reg)
            h->reg = *reg;
        else {
            pkg_registry_load(&h->reg);
            if (h->reg.count == 0)
                pkg_seed_from_dir("/apps", &h->reg);
        }
    }
    return 0;
}

static void normalize_layout_rel(const char *in, char *out, size_t n)
{
    if (!in || !in[0]) {
        str_copy(out, n, "res/layout/main.xml");
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
    if (str_starts(in, "/apps/")) {
        const char *p = strstr(in, "/res/layout/");
        if (p) {
            str_copy(out, n, p + 1);
            return;
        }
    }
    str_copy(out, n, in);
}

static void after_layout(AppHost *h, Screen *s, Gfx *g)
{
    if (!h || !s)
        return;
    if (strstr(s->path, "home.xml") || strstr(s->path, "drawer.xml") ||
        strstr(s->path, "desktop.xaml") || strstr(s->path, "dashboard.xaml"))
        apphost_refresh_launcher(h, s);
    if (strstr(s->path, "recents.xml"))
        apphost_refresh_recents(h, s);
    if (strstr(s->path, "home.xml") || strstr(s->path, "desktop.xaml") ||
        strstr(s->path, "dashboard.xaml") || strstr(s->path, "taskbar.xaml"))
        wm_sync_taskbar(&g_wm, s);
    if (g)
        screen_layout(s, g);
    apphost_update_softkeys(h, s);
}

static int load_layout(AppHost *h, Screen *s, Gfx *g, const PkgApp *app, const char *screen_rel, int restore_focus)
{
    char ui[MAX_PATH];
    char script[MAX_PATH];
    char rel[MAX_PATH];
    normalize_layout_rel(screen_rel && screen_rel[0] ? screen_rel : app->entry_screen, rel, sizeof(rel));
    snprintf(ui, sizeof(ui), "%s/%s/%s", PKG_APPS_ROOT, app->id, rel);
    snprintf(script, sizeof(script), "%s/%s/%s", PKG_APPS_ROOT, app->id,
             app->entry_script[0] ? app->entry_script : "bin/main.kbc");
    {
        FILE *probe = fopen(script, "r");
        if (!probe) {
            snprintf(script, sizeof(script), "%s/%s/bin/main.kbc", PKG_APPS_ROOT, app->id);
            probe = fopen(script, "r");
        }
        if (!probe) {
            snprintf(script, sizeof(script), "%s/%s/src/main.kt", PKG_APPS_ROOT, app->id);
            probe = fopen(script, "r");
        }
        if (probe)
            fclose(probe);
    }
    if (screen_load(s, ui) != 0) {
        char fallback[MAX_PATH];
        snprintf(fallback, sizeof(fallback), "assets/screens/%s.xml", app->id);
        if (screen_load(s, fallback) != 0)
            return -1;
        vsl_reset(&h->vsl);
        h->vsl.app = *app;
        h->vsl.screen = s;
        h->vsl.loaded = 1;
    } else {
        h->current = *app;
        h->in_app = 1;
        after_layout(h, s, g);
        vsl_load(&h->vsl, app, s, script);
        if (restore_focus >= 0 && restore_focus < s->count && s->widgets[restore_focus].focusable)
            screen_set_focus(s, restore_focus);
        return 0;
    }
    h->current = *app;
    h->in_app = 1;
    if (restore_focus >= 0 && restore_focus < s->count && s->widgets[restore_focus].focusable)
        screen_set_focus(s, restore_focus);
    after_layout(h, s, g);
    return 0;
}

static int resolve_app(AppHost *h, const char *id, PkgApp *app)
{
    memset(app, 0, sizeof(*app));
    if (!id || !id[0])
        return -1;
    pkg_manager_refresh();
    if (pkg_manager_find(id, app) == 0)
        return 0;
    int idx = pkg_registry_find(&h->reg, id);
    if (idx >= 0) {
        *app = h->reg.apps[idx];
        return 0;
    }
    char man[MAX_PATH];
    snprintf(man, sizeof(man), "%s/%s/manifest.xml", PKG_APPS_ROOT, id);
    if (pkg_parse_manifest(man, app) != 0) {
        str_copy(app->id, sizeof(app->id), id);
        str_copy(app->name, sizeof(app->name), id);
        str_copy(app->entry_screen, sizeof(app->entry_screen), "res/layout/main.xml");
        str_copy(app->entry_script, sizeof(app->entry_script), "bin/main.kbc");
        str_copy(app->icon, sizeof(app->icon), id);
    }
    return 0;
}

static void build_script_path(const PkgApp *app, char *script, size_t n)
{
    snprintf(script, n, "%s/%s/%s", PKG_APPS_ROOT, app->id,
             app->entry_script[0] ? app->entry_script : "bin/main.kbc");
    FILE *probe = fopen(script, "r");
    if (!probe) {
        snprintf(script, n, "%s/%s/bin/main.kbc", PKG_APPS_ROOT, app->id);
        probe = fopen(script, "r");
    }
    if (!probe) {
        snprintf(script, n, "%s/%s/src/main.kt", PKG_APPS_ROOT, app->id);
        probe = fopen(script, "r");
    }
    if (probe)
        fclose(probe);
}

static int softkey_clearable_input(const Widget *w)
{
    if (!w || w->type != W_INPUT)
        return 0;
    return str_eq(w->id, "number") || str_eq(w->id, "note") || str_eq(w->id, "msg") ||
           str_eq(w->id, "msg_input");
}

void apphost_update_softkeys(AppHost *h, Screen *s)
{
    (void)h;
    if (!s)
        return;
    Widget *inp = NULL;
    if (s->focus >= 0 && softkey_clearable_input(&s->widgets[s->focus]))
        inp = &s->widgets[s->focus];
    if (!inp)
        inp = screen_find(s, "number");
    if (!inp || !softkey_clearable_input(inp))
        return;
    int has = inp->text[0] != 0;
    if (str_eq(s->soft_right, "Clear") || str_eq(s->soft_right, "Back") ||
        str_eq(s->soft_right_act, "clear") || str_eq(s->soft_right_act, "route:pop") ||
        str_eq(s->soft_right_act, "back")) {
        if (has) {
            str_copy(s->soft_right, sizeof(s->soft_right), "Clear");
            str_copy(s->soft_right_act, sizeof(s->soft_right_act), "clear");
        } else {
            str_copy(s->soft_right, sizeof(s->soft_right), "Back");
            str_copy(s->soft_right_act, sizeof(s->soft_right_act), "route:pop");
        }
    }
}

int apphost_open_shell(AppHost *h, Screen *s, Gfx *g, const char *layout)
{
    PkgApp app;
    if (!h || !s)
        return -1;
    resolve_app(h, h->shell_app[0] ? h->shell_app : "launcher", &app);
    char rel[MAX_PATH];
    if (layout && layout[0])
        normalize_layout_rel(layout, rel, sizeof(rel));
    else
        str_copy(rel, sizeof(rel), "xaml/desktop.xaml");
    h->unlocked = 1;
    route_reset(&h->stack);
    route_push(&h->stack, app.id, rel, -1, NULL);
    return load_layout(h, s, g, &app, rel, -1);
}

int apphost_open_app(AppHost *h, Screen *s, Gfx *g, const char *id)
{
    if (!h || !s || !id)
        return -1;
    if (is_shell_app(h, id))
        return apphost_open_shell(h, s, g, "xaml/desktop.xaml");

    OsWindow *prev = wm_focused(&g_wm);
    if (prev && !is_shell_app(h, prev->app_id))
        save_host_to_window(h, prev);

    PkgApp app;
    resolve_app(h, id, &app);
    apphost_push_recent(h, id);

    OsWindow *existing = wm_find_app(&g_wm, id);
    if (existing) {
        wm_focus(&g_wm, existing->id);
        sync_host_from_window(h, existing);
        if (existing->script[0])
            vsl_load(&h->vsl, &existing->app, &existing->screen, existing->script);
        wm_sync_taskbar(&g_wm, g_wm.desktop ? g_wm.desktop : s);
        return 0;
    }

    char script[MAX_PATH];
    build_script_path(&app, script, sizeof(script));
    int wid = wm_open(&g_wm, &app, script, app.name[0] ? app.name : app.id);
    OsWindow *win = wm_find(&g_wm, wid);
    if (!win)
        return -1;

    char rel[MAX_PATH];
    normalize_layout_rel(app.entry_screen, rel, sizeof(rel));
    route_reset(&win->stack);
    route_push(&win->stack, app.id, rel, -1, NULL);
    h->stack = win->stack;
    int rc = load_layout(h, &win->screen, g, &app, rel, -1);
    win->stack = h->stack;
    str_copy(win->script, sizeof(win->script), script);
    wm_layout_window(&g_wm, win);
    wm_sync_taskbar(&g_wm, g_wm.desktop ? g_wm.desktop : s);
    return rc;
}

int apphost_route_push(AppHost *h, Screen *s, Gfx *g, const char *layout)
{
    if (!h || !h->in_app || !layout)
        return -1;
    Screen *target = s;
    OsWindow *win = NULL;
    if (!is_shell_app(h, h->current.id)) {
        win = wm_find_app(&g_wm, h->current.id);
        if (win)
            target = &win->screen;
    }
    char rel[MAX_PATH];
    normalize_layout_rel(layout, rel, sizeof(rel));
    route_save_focus(&h->stack, target ? target->focus : -1);
    if (route_push(&h->stack, h->current.id, rel, -1, NULL) != 0)
        return -1;
    int rc = load_layout(h, target, g, &h->current, rel, -1);
    if (win) {
        win->stack = h->stack;
        wm_layout_window(&g_wm, win);
    }
    return rc;
}

int apphost_route_replace(AppHost *h, Screen *s, Gfx *g, const char *layout)
{
    if (!h || !h->in_app || !layout)
        return -1;
    Screen *target = s;
    OsWindow *win = NULL;
    if (!is_shell_app(h, h->current.id)) {
        win = wm_find_app(&g_wm, h->current.id);
        if (win)
            target = &win->screen;
    }
    char rel[MAX_PATH];
    normalize_layout_rel(layout, rel, sizeof(rel));
    if (strstr(rel, "home.xml") && is_shell_app(h, h->current.id))
        h->unlocked = 1;
    if (route_depth(&h->stack) <= 0) {
        route_push(&h->stack, h->current.id, rel, -1, NULL);
    } else {
        route_replace(&h->stack, rel, -1, NULL);
    }
    int rc = load_layout(h, target, g, &h->current, rel, -1);
    if (win) {
        win->stack = h->stack;
        wm_layout_window(&g_wm, win);
    }
    if (is_shell_app(h, h->current.id) &&
        (strstr(rel, "home.xml") || strstr(rel, "desktop.xaml")) && g_wm.desktop) {
        wm_set_work_area(&g_wm, 24, 8, g && g->w > 48 ? g->w - 48 : (g ? g->w : 0),
                         g && g->h > 64 ? g->h - 64 : (g ? g->h : 0));
        wm_sync_taskbar(&g_wm, g_wm.desktop);
    }
    return rc;
}

int apphost_route_pop(AppHost *h, Screen *s, Gfx *g, char *out_nav, size_t out_sz)
{
    if (!h)
        return -1;
    if (!h->in_app) {
        if (out_nav)
            out_nav[0] = 0;
        return 0;
    }
    if (route_is_root(&h->stack)) {
        if (is_shell_app(h, h->current.id)) {
            if (out_nav)
                out_nav[0] = 0;
            return 0;
        }
        wm_close_app(&g_wm, h->current.id);
        bind_shell_host(h, g_wm.desktop ? g_wm.desktop : s);
        route_reset(&h->stack);
        if (g_wm.desktop) {
            route_push(&h->stack, h->current.id, "xaml/desktop.xaml", -1, NULL);
            wm_sync_taskbar(&g_wm, g_wm.desktop);
        }
        vsl_reset(&h->vsl);
        if (out_nav)
            out_nav[0] = 0;
        return 2;
    }
    RouteFrame discarded;
    route_pop(&h->stack, &discarded);
    const RouteFrame *top = route_top(&h->stack);
    if (!top) {
        if (is_shell_app(h, h->current.id)) {
            if (out_nav)
                out_nav[0] = 0;
            return 0;
        }
        wm_close_app(&g_wm, h->current.id);
        bind_shell_host(h, g_wm.desktop ? g_wm.desktop : s);
        route_reset(&h->stack);
        wm_sync_taskbar(&g_wm, g_wm.desktop ? g_wm.desktop : s);
        vsl_reset(&h->vsl);
        if (out_nav)
            out_nav[0] = 0;
        return 2;
    }
    Screen *target = s;
    OsWindow *win = NULL;
    if (!is_shell_app(h, h->current.id)) {
        win = wm_find_app(&g_wm, h->current.id);
        if (win)
            target = &win->screen;
    }
    if (load_layout(h, target, g, &h->current, top->layout, top->focus) != 0)
        return -1;
    if (win) {
        win->stack = h->stack;
        wm_layout_window(&g_wm, win);
    }
    if (out_nav)
        out_nav[0] = 0;
    return 2;
}

int apphost_route_home(AppHost *h, Screen *s, Gfx *g, char *out_nav, size_t out_sz)
{
    if (!h)
        return -1;
    OsWindow *prev = wm_focused(&g_wm);
    if (prev && !is_shell_app(h, prev->app_id))
        save_host_to_window(h, prev);
    wm_handle_action(&g_wm, "wm:show_desktop");
    Screen *desktop = g_wm.desktop ? g_wm.desktop : s;
    const char *layout = "xaml/desktop.xaml";
    h->unlocked = 1;
    if (h->in_app && is_shell_app(h, h->current.id)) {
        route_reset(&h->stack);
        route_push(&h->stack, h->current.id, layout, -1, NULL);
        if (load_layout(h, desktop, g, &h->current, layout, -1) != 0)
            return -1;
        wm_sync_taskbar(&g_wm, desktop);
        if (out_nav)
            out_nav[0] = 0;
        return 2;
    }
    if (apphost_open_shell(h, desktop, g, layout) != 0) {
        if (out_nav)
            str_copy(out_nav, out_sz, h->shell_path);
        return 1;
    }
    wm_sync_taskbar(&g_wm, desktop);
    if (out_nav)
        out_nav[0] = 0;
    return 2;
}

int apphost_open_screen(AppHost *h, Screen *s, Gfx *g, const char *rel_or_abs)
{
    if (!h || !s || !rel_or_abs)
        return -1;
    if (str_starts(rel_or_abs, "assets/")) {
        if (screen_load(s, rel_or_abs) != 0)
            return -1;
        route_reset(&h->stack);
        h->in_app = 0;
        vsl_reset(&h->vsl);
        if (strstr(rel_or_abs, "home.xml"))
            apphost_refresh_launcher(h, s);
        if (g)
            screen_layout(s, g);
        return 0;
    }
    if (h->in_app)
        return apphost_route_push(h, s, g, rel_or_abs);
    return -1;
}

int apphost_fire_event(AppHost *h, const char *event, const char *target)
{
    if (!h || !h->in_app)
        return 0;
    return vsl_fire(&h->vsl, event, target ? target : "");
}

int apphost_on_tick(AppHost *h)
{
    if (!h || !h->in_app)
        return 0;
    return vsl_fire(&h->vsl, "tick", "");
}

static int handle_route_action(AppHost *h, Screen *s, Gfx *g, const char *action, char *out_nav, size_t out_sz)
{
    if (str_starts(action, "route:push:"))
        return apphost_route_push(h, s, g, action + 11) == 0 ? 2 : -1;
    if (str_starts(action, "route:replace:"))
        return apphost_route_replace(h, s, g, action + 14) == 0 ? 2 : -1;
    if (str_eq(action, "route:pop") || str_eq(action, "pop_route") || str_eq(action, "back"))
        return apphost_route_pop(h, s, g, out_nav, out_sz);
    if (str_eq(action, "route:home") || str_eq(action, "go_home") || str_eq(action, "home"))
        return apphost_route_home(h, s, g, out_nav, out_sz);
    return 0;
}

static const char *vapi_method_for(const char *action)
{
    if (str_eq(action, "call"))
        return "telephony.call";
    if (str_eq(action, "end_call"))
        return "telephony.end";
    if (str_eq(action, "mute"))
        return "telephony.mute";
    if (str_eq(action, "speaker"))
        return "telephony.speaker";
    if (str_eq(action, "send"))
        return "messages.send";
    return NULL;
}

int apphost_handle_action(AppHost *h, Screen *s, Gfx *g, const char *action, char *out_nav, size_t out_sz)
{
    if (!h || !action || !action[0])
        return 0;
    if (str_eq(action, "power_off"))
        return 30;
    if (str_eq(action, "power_restart"))
        return 31;

    if (str_starts(action, "wm:")) {
        OsWindow *prev = wm_focused(&g_wm);
        if (prev && !is_shell_app(h, prev->app_id))
            save_host_to_window(h, prev);
        int wrc = wm_handle_action(&g_wm, action);
        OsWindow *now = wm_focused(&g_wm);
        if (now) {
            sync_host_from_window(h, now);
            if (now->script[0])
                vsl_load(&h->vsl, &now->app, &now->screen, now->script);
        } else {
            bind_shell_host(h, g_wm.desktop ? g_wm.desktop : s);
        }
        wm_sync_taskbar(&g_wm, g_wm.desktop ? g_wm.desktop : s);
        if (out_nav)
            out_nav[0] = 0;
        return wrc >= 0 ? 2 : -1;
    }

    if (str_eq(action, "unlock") && h->in_app && is_shell_app(h, h->current.id)) {
        h->unlocked = 1;
        if (apphost_route_replace(h, s, g, "xaml/desktop.xaml") == 0) {
            if (out_nav)
                out_nav[0] = 0;
            return 2;
        }
        return -1;
    }

    if (str_eq(action, "shade")) {
        if (h->in_app && is_shell_app(h, h->current.id)) {
            if (apphost_route_push(h, s, g, "res/layout/shade.xml") == 0) {
                if (out_nav)
                    out_nav[0] = 0;
                return 2;
            }
            return -1;
        }
        if (h->in_app && is_shell_app(h, h->shell_app)) {
            return -1;
        }
    }

    if (str_eq(action, "clear") && h->in_app && is_shell_app(h, h->current.id) &&
        s && strstr(s->path, "recents.xml")) {
        apphost_clear_recents(h);
        apphost_refresh_recents(h, s);
        if (g)
            screen_layout(s, g);
        if (out_nav)
            out_nav[0] = 0;
        return 2;
    }

    if (str_starts(action, "recent_") && h->in_app && is_shell_app(h, h->current.id)) {
        int idx = atoi(action + 7);
        if (idx >= 0 && idx < h->recent_count) {
            if (apphost_open_app(h, s, g, h->recents[idx]) == 0) {
                if (out_nav)
                    out_nav[0] = 0;
                return 2;
            }
            return -1;
        }
        return apphost_route_pop(h, s, g, out_nav, out_sz);
    }

    int rc = handle_route_action(h, s, g, action, out_nav, out_sz);
    if (rc != 0)
        return rc;

    if (str_starts(action, "app:")) {
        if (apphost_open_app(h, s, g, action + 4) == 0) {
            if (out_nav)
                out_nav[0] = 0;
            return 2;
        }
        return -1;
    }

    if (str_starts(action, "nav:")) {
        if (h->in_app)
            return apphost_route_push(h, s, g, action + 4) == 0 ? 2 : -1;
        if (out_nav)
            str_copy(out_nav, out_sz, action + 4);
        return 1;
    }

    if (str_starts(action, "key:")) {
        const char *key = action + 4;
        char taken[MAX_PATH];
        taken[0] = 0;
        int handled = 0;
        if (h->in_app)
            handled = apphost_fire_event(h, "number", key);
        if (!handled) {
            char dig_act[16];
            snprintf(dig_act, sizeof(dig_act), "digit:%s", key);
            int rc = screen_handle_action(s, dig_act, taken, sizeof(taken));
            if (rc == 1 && taken[0])
                return apphost_handle_action(h, s, g, taken, out_nav, out_sz);
        }
        if (vsl_consume_nav(&h->vsl, taken, sizeof(taken)))
            return apphost_handle_action(h, s, g, taken, out_nav, out_sz);
        apphost_update_softkeys(h, s);
        if (out_nav)
            out_nav[0] = 0;
        return 2;
    }

    if (h->in_app && apphost_fire_event(h, "action", action) > 0) {
        char taken[MAX_PATH];
        if (vsl_consume_nav(&h->vsl, taken, sizeof(taken)))
            return apphost_handle_action(h, s, g, taken, out_nav, out_sz);
        apphost_update_softkeys(h, s);
        if (out_nav)
            out_nav[0] = 0;
        return 2;
    }

    if (h->in_app && s && s->focus >= 0) {
        Widget *w = &s->widgets[s->focus];
        if (str_eq(action, "ok") || str_eq(action, "select")) {
            if (w->action[0] && str_starts(w->action, "key:"))
                return apphost_handle_action(h, s, g, w->action, out_nav, out_sz);
            apphost_fire_event(h, "click", w->id);
            char taken[MAX_PATH];
            if (vsl_consume_nav(&h->vsl, taken, sizeof(taken)))
                return apphost_handle_action(h, s, g, taken, out_nav, out_sz);
            if (w->action[0] && !str_eq(w->action, "none"))
                return apphost_handle_action(h, s, g, w->action, out_nav, out_sz);
        }
    }

    const char *method = h->in_app ? vapi_method_for(action) : NULL;
    if (method) {
        const char *status = NULL;
        if (str_eq(action, "send"))
            status = screen_find(s, "msgstatus") ? "msgstatus" : "status_line";
        if (bridge_exec_cmd(&h->vsl, method, NULL, status) == 0) {
            char taken[MAX_PATH];
            if (vsl_consume_nav(&h->vsl, taken, sizeof(taken)))
                return apphost_handle_action(h, s, g, taken, out_nav, out_sz);
            apphost_update_softkeys(h, s);
            if (out_nav)
                out_nav[0] = 0;
            return 2;
        }
    }

    char local_nav[MAX_PATH];
    local_nav[0] = 0;
    rc = screen_handle_action(s, action, local_nav, sizeof(local_nav));
    if (rc == 1 && local_nav[0]) {
        if (str_starts(local_nav, "route:") || str_eq(local_nav, "back") || str_eq(local_nav, "home"))
            return apphost_handle_action(h, s, g, local_nav, out_nav, out_sz);
        if (h->in_app && !str_starts(local_nav, "assets/") && !str_starts(local_nav, "app:"))
            return apphost_route_push(h, s, g, local_nav) == 0 ? 2 : -1;
        if (out_nav)
            str_copy(out_nav, out_sz, local_nav);
        return 1;
    }
    if (rc == 2)
        apphost_update_softkeys(h, s);
    return rc;
}
