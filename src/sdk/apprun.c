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

static int load_app_screen(AppHost *h, Screen *s, Gfx *g, const PkgApp *app, const char *screen_rel)
{
    char ui[MAX_PATH];
    char script[MAX_PATH];
    snprintf(ui, sizeof(ui), "%s/%s/%s", PKG_APPS_ROOT, app->id, screen_rel && screen_rel[0] ? screen_rel : app->entry_screen);
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
    return 0;
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
            str_copy(app.entry_screen, sizeof(app.entry_screen), "ui/main.xml");
            str_copy(app.entry_script, sizeof(app.entry_script), "logic/main.vsl");
            str_copy(app.icon, sizeof(app.icon), id);
        }
    }
    return load_app_screen(h, s, g, &app, app.entry_screen);
}

int apphost_open_screen(AppHost *h, Screen *s, Gfx *g, const char *rel_or_abs)
{
    if (!h || !s || !rel_or_abs)
        return -1;
    if (str_starts(rel_or_abs, "assets/") || str_starts(rel_or_abs, "/apps/")) {
        if (screen_load(s, rel_or_abs) != 0)
            return -1;
        if (strstr(rel_or_abs, "home.xml")) {
            h->in_app = 0;
            vsl_reset(&h->vsl);
            apphost_refresh_launcher(h, s);
        } else if (strstr(rel_or_abs, "/apps/") && h->in_app) {
            h->vsl.screen = s;
            char script[MAX_PATH];
            snprintf(script, sizeof(script), "%s/%s/%s", PKG_APPS_ROOT, h->current.id, h->current.entry_script);
            vsl_load(&h->vsl, &h->current, s, script);
        } else {
            h->in_app = 0;
            vsl_reset(&h->vsl);
        }
        if (g)
            screen_layout(s, g);
        return 0;
    }
    if (h->in_app)
        return load_app_screen(h, s, g, &h->current, rel_or_abs);
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

int apphost_handle_action(AppHost *h, Screen *s, Gfx *g, const char *action, char *out_nav, size_t out_sz)
{
    if (!h || !action)
        return 0;
    if (str_starts(action, "app:")) {
        if (apphost_open_app(h, s, g, action + 4) == 0) {
            if (out_nav)
                out_nav[0] = 0;
            return 2;
        }
        return -1;
    }
    if (h->in_app && s->focus >= 0) {
        Widget *w = &s->widgets[s->focus];
        if (str_eq(action, "ok") || str_eq(action, "select")) {
            apphost_fire_event(h, "click", w->id);
            if (vsl_has_nav(&h->vsl)) {
                const char *nav = vsl_take_nav(&h->vsl);
                if (str_eq(nav, "shell:back")) {
                    h->in_app = 0;
                    vsl_reset(&h->vsl);
                    if (out_nav)
                        str_copy(out_nav, out_sz, h->shell_path);
                    h->vsl.nav[0] = 0;
                    return 1;
                }
                if (str_starts(nav, "assets/") || str_starts(nav, "/apps/") || str_starts(nav, "app:")) {
                    if (out_nav)
                        str_copy(out_nav, out_sz, nav);
                    h->vsl.nav[0] = 0;
                    return 1;
                }
                apphost_open_screen(h, s, g, nav);
                h->vsl.nav[0] = 0;
                return 2;
            }
            if (w->action[0] && !str_eq(w->action, "none")) {
                if (str_starts(w->action, "app:"))
                    return apphost_handle_action(h, s, g, w->action, out_nav, out_sz);
            }
        }
    }
    if (str_eq(action, "back") && h->in_app) {
        apphost_fire_event(h, "soft", "right");
        if (vsl_has_nav(&h->vsl)) {
            const char *nav = vsl_take_nav(&h->vsl);
            if (str_eq(nav, "shell:back") || str_starts(nav, "assets/screens/home")) {
                h->in_app = 0;
                vsl_reset(&h->vsl);
                if (out_nav)
                    str_copy(out_nav, out_sz, "assets/screens/home.xml");
                return 1;
            }
        }
        h->in_app = 0;
        vsl_reset(&h->vsl);
        if (out_nav)
            str_copy(out_nav, out_sz, "assets/screens/home.xml");
        return 1;
    }
    return screen_handle_action(s, action, out_nav, out_sz);
}
