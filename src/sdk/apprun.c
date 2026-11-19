#include "apprun.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

int apphost_init(AppHost *h)
{
    if (!h)
        return -1;
    memset(h, 0, sizeof(*h));
    str_copy(h->shell_path, sizeof(h->shell_path), "assets/screens/home.xml");
    route_reset(&h->stack);
    pkg_registry_load(&h->reg);
    if (h->reg.count == 0)
        pkg_seed_from_dir("/apps", &h->reg);
    return 0;
}

int apphost_refresh_launcher(AppHost *h, Screen *s)
{
    if (!h || !s)
        return -1;
    Widget *grid = screen_find(s, "launcher");
    if (!grid || grid->type != W_GRID)
        return 0;
    int wi = 0;
    for (int i = 0; i < grid->child_count && wi < h->reg.count; i++) {
        Widget *b = &s->widgets[grid->children[i]];
        if (b->type != W_BUTTON)
            continue;
        const PkgApp *a = &h->reg.apps[wi++];
        str_copy(b->text, sizeof(b->text), a->name);
        str_copy(b->icon, sizeof(b->icon), a->icon[0] ? a->icon : a->id);
        snprintf(b->action, sizeof(b->action), "app:%s", a->id);
        b->visible = 1;
        b->focusable = 1;
    }
    for (int i = 0; i < grid->child_count; i++) {
        Widget *b = &s->widgets[grid->children[i]];
        if (b->type != W_BUTTON)
            continue;
        int used = 0;
        for (int k = 0; k < h->reg.count; k++) {
            char act[64];
            snprintf(act, sizeof(act), "app:%s", h->reg.apps[k].id);
            if (str_eq(b->action, act)) {
                used = 1;
                break;
            }
        }
        if (!used && str_starts(b->action, "app:")) {
            b->visible = 0;
            b->focusable = 0;
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

static int load_layout(AppHost *h, Screen *s, Gfx *g, const PkgApp *app, const char *screen_rel, int restore_focus)
{
    char ui[MAX_PATH];
    char script[MAX_PATH];
    char rel[MAX_PATH];
    normalize_layout_rel(screen_rel && screen_rel[0] ? screen_rel : app->entry_screen, rel, sizeof(rel));
    snprintf(ui, sizeof(ui), "%s/%s/%s", PKG_APPS_ROOT, app->id, rel);
    snprintf(script, sizeof(script), "%s/%s/%s", PKG_APPS_ROOT, app->id, app->entry_script);
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
        vsl_load(&h->vsl, app, s, script);
    }
    h->current = *app;
    h->in_app = 1;
    if (g)
        screen_layout(s, g);
    if (restore_focus >= 0 && restore_focus < s->count && s->widgets[restore_focus].focusable)
        screen_set_focus(s, restore_focus);
    apphost_update_softkeys(h, s);
    return 0;
}

static int softkey_clearable_input(const Widget *w)
{
    if (!w || w->type != W_INPUT)
        return 0;
    return str_eq(w->id, "number") || str_eq(w->id, "note") || str_eq(w->id, "msg");
}

void apphost_update_softkeys(AppHost *h, Screen *s)
{
    (void)h;
    if (!s)
        return;
    /* Only dialer/notes/compose fields toggle Clear↔Back — never browser URL etc. */
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

int apphost_open_app(AppHost *h, Screen *s, Gfx *g, const char *id)
{
    if (!h || !s || !id)
        return -1;
    int idx = pkg_registry_find(&h->reg, id);
    PkgApp app;
    memset(&app, 0, sizeof(app));
    if (idx >= 0) {
        app = h->reg.apps[idx];
    } else {
        char man[MAX_PATH];
        snprintf(man, sizeof(man), "%s/%s/manifest.xml", PKG_APPS_ROOT, id);
        if (pkg_parse_manifest(man, &app) != 0) {
            str_copy(app.id, sizeof(app.id), id);
            str_copy(app.name, sizeof(app.name), id);
            str_copy(app.entry_screen, sizeof(app.entry_screen), "res/layout/main.xml");
            str_copy(app.entry_script, sizeof(app.entry_script), "src/main.vsl");
            str_copy(app.icon, sizeof(app.icon), id);
        }
    }
    route_reset(&h->stack);
    char rel[MAX_PATH];
    normalize_layout_rel(app.entry_screen, rel, sizeof(rel));
    route_push(&h->stack, app.id, rel, -1, NULL);
    return load_layout(h, s, g, &app, rel, -1);
}

int apphost_route_push(AppHost *h, Screen *s, Gfx *g, const char *layout)
{
    if (!h || !h->in_app || !layout)
        return -1;
    char rel[MAX_PATH];
    normalize_layout_rel(layout, rel, sizeof(rel));
    route_save_focus(&h->stack, s ? s->focus : -1);
    if (route_push(&h->stack, h->current.id, rel, -1, NULL) != 0)
        return -1;
    return load_layout(h, s, g, &h->current, rel, -1);
}

int apphost_route_replace(AppHost *h, Screen *s, Gfx *g, const char *layout)
{
    if (!h || !h->in_app || !layout)
        return -1;
    char rel[MAX_PATH];
    normalize_layout_rel(layout, rel, sizeof(rel));
    if (route_depth(&h->stack) <= 0) {
        route_push(&h->stack, h->current.id, rel, -1, NULL);
    } else {
        route_replace(&h->stack, rel, -1, NULL);
    }
    return load_layout(h, s, g, &h->current, rel, -1);
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
        route_reset(&h->stack);
        h->in_app = 0;
        vsl_reset(&h->vsl);
        if (out_nav)
            str_copy(out_nav, out_sz, h->shell_path);
        return 1;
    }
    RouteFrame discarded;
    route_pop(&h->stack, &discarded);
    const RouteFrame *top = route_top(&h->stack);
    if (!top) {
        route_reset(&h->stack);
        h->in_app = 0;
        vsl_reset(&h->vsl);
        if (out_nav)
            str_copy(out_nav, out_sz, h->shell_path);
        return 1;
    }
    if (load_layout(h, s, g, &h->current, top->layout, top->focus) != 0)
        return -1;
    if (out_nav)
        out_nav[0] = 0;
    return 2;
}

int apphost_route_home(AppHost *h, Screen *s, Gfx *g, char *out_nav, size_t out_sz)
{
    (void)s;
    (void)g;
    if (!h)
        return -1;
    route_reset(&h->stack);
    h->in_app = 0;
    vsl_reset(&h->vsl);
    if (out_nav)
        str_copy(out_nav, out_sz, h->shell_path);
    return 1;
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

int apphost_handle_action(AppHost *h, Screen *s, Gfx *g, const char *action, char *out_nav, size_t out_sz)
{
    if (!h || !action || !action[0])
        return 0;
    if (str_eq(action, "power_off"))
        return 30;
    if (str_eq(action, "power_restart"))
        return 31;

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

    if (h->in_app && s && s->focus >= 0) {
        Widget *w = &s->widgets[s->focus];
        if (str_eq(action, "ok") || str_eq(action, "select")) {
            apphost_fire_event(h, "click", w->id);
            char taken[MAX_PATH];
            if (vsl_consume_nav(&h->vsl, taken, sizeof(taken)))
                return apphost_handle_action(h, s, g, taken, out_nav, out_sz);
            if (w->action[0] && !str_eq(w->action, "none"))
                return apphost_handle_action(h, s, g, w->action, out_nav, out_sz);
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
