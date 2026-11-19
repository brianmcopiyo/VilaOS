#include "types.h"
#include "util.h"
#include "fb.h"
#include "draw.h"
#include "font.h"
#include "screen.h"
#include "input.h"
#include "apprun.h"
#include "pkg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static volatile int g_run = 1;
static AppHost g_host;

static void on_signal(int sig)
{
    (void)sig;
    g_run = 0;
}

static void activate_focus(Screen *s, Gfx *g, char *nav, size_t nav_sz)
{
    if (s->focus < 0)
        return;
    Widget *w = &s->widgets[s->focus];
    if (g_host.in_app) {
        apphost_fire_event(&g_host, "click", w->id);
        char taken[MAX_PATH];
        if (vsl_consume_nav(&g_host.vsl, taken, sizeof(taken))) {
            apphost_handle_action(&g_host, s, g, taken, nav, nav_sz);
            return;
        }
    }
    if (w->action[0] && !str_eq(w->action, "none"))
        apphost_handle_action(&g_host, s, g, w->action, nav, nav_sz);
}

static Widget *focused_input(Screen *s)
{
    if (s->focus >= 0 && s->widgets[s->focus].type == W_INPUT)
        return &s->widgets[s->focus];
    Widget *n = screen_find(s, "number");
    if (n)
        return n;
    Widget *msg = screen_find(s, "msg");
    if (msg)
        return msg;
    Widget *note = screen_find(s, "note");
    if (note)
        return note;
    Widget *to = screen_find(s, "to");
    if (to)
        return to;
    return NULL;
}

static void apply_text(Screen *s, InputMux *mux, InputEvent *ev)
{
    (void)mux;
    Widget *w = focused_input(s);
    if (!w)
        return;
    if (ev->x == 1) {
        size_t n = strlen(w->text);
        if (n > 0)
            w->text[n - 1] = 0;
    }
    input_apply_char(w, ev->ch);
    if (g_host.in_app)
        apphost_fire_event(&g_host, "change", w->id);
    apphost_update_softkeys(&g_host, s);
}

static int path_is_dialer(const char *path)
{
    return path && (strstr(path, "dialer") != NULL);
}

static void sync_bounds(InputMux *input, Gfx *gfx)
{
    input->state.bound_w = gfx->w;
    input->state.bound_h = gfx->h;
    input->state.cursor_x = clamp_i(input->state.cursor_x, 0, gfx->w > 0 ? gfx->w - 1 : 0);
    input->state.cursor_y = clamp_i(input->state.cursor_y, 0, gfx->h > 0 ? gfx->h - 1 : 0);
}

static int apply_nav(Screen *s, Gfx *g, InputMux *input, char *nav)
{
    if (!nav[0])
        return 0;
    if (str_starts(nav, "route:") || str_eq(nav, "back") || str_eq(nav, "home") ||
        str_eq(nav, "go_home") || str_eq(nav, "pop_route")) {
        char next[MAX_PATH];
        next[0] = 0;
        int rc = apphost_handle_action(&g_host, s, g, nav, next, sizeof(next));
        if (rc == 1 && next[0]) {
            str_copy(nav, MAX_PATH, next);
        } else {
            input_set_dialer_mode(input, path_is_dialer(s->path));
            nav[0] = 0;
            return rc >= 0 ? 1 : -1;
        }
    }
    if (str_starts(nav, "app:")) {
        if (apphost_open_app(&g_host, s, g, nav + 4) == 0) {
            input_set_dialer_mode(input, path_is_dialer(s->path));
            nav[0] = 0;
            return 1;
        }
        return -1;
    }
    if (str_starts(nav, "/apps/")) {
        if (apphost_open_screen(&g_host, s, g, nav) == 0) {
            input_set_dialer_mode(input, path_is_dialer(s->path));
            nav[0] = 0;
            return 1;
        }
        return -1;
    }
    if (screen_load(s, nav) != 0)
        return -1;
    if (strstr(nav, "home.xml")) {
        apphost_route_home(&g_host, s, g, NULL, 0);
        apphost_refresh_launcher(&g_host, s);
    } else if (strstr(nav, "idle.xml")) {
        apphost_route_home(&g_host, s, g, NULL, 0);
    }
    screen_layout(s, g);
    input_set_dialer_mode(input, path_is_dialer(s->path));
    nav[0] = 0;
    return 1;
}

/* side: 1=left, 2=center, 3=right */
static int run_soft(Screen *s, Gfx *g, int side, const char *act, char *nav, size_t nav_sz)
{
    if (!act || !act[0])
        return 0;
    char taken[MAX_PATH];
    taken[0] = 0;
    if (g_host.in_app) {
        const char *tgt = side == 1 ? "left" : (side == 2 ? "center" : "right");
        apphost_fire_event(&g_host, "soft", tgt);
        vsl_consume_nav(&g_host.vsl, taken, sizeof(taken));
    }
    /* Softkey Clear must clear text even if VSL on_soft right always pops. */
    if (str_eq(act, "clear"))
        return apphost_handle_action(&g_host, s, g, act, nav, nav_sz);
    if (taken[0])
        return apphost_handle_action(&g_host, s, g, taken, nav, nav_sz);
    return apphost_handle_action(&g_host, s, g, act, nav, nav_sz);
}

int main(int argc, char **argv)
{
    int desktop = 0;
    const char *start = "assets/screens/idle.xml";
    const char *env_start = getenv("VILAOS_SCREEN");
    if (env_start && env_start[0])
        start = env_start;
    for (int i = 1; i < argc; i++) {
        if (str_eq(argv[i], "--desktop") || str_eq(argv[i], "-d"))
            desktop = 1;
        else if (str_eq(argv[i], "--screen") && i + 1 < argc)
            start = argv[++i];
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    Gfx gfx;
    if (gfx_init(&gfx, desktop) != 0) {
        fprintf(stderr, "gfx_init failed\n");
        return 1;
    }

    InputMux input;
    if (input_init(&input) != 0)
        fprintf(stderr, "input_init failed (continuing without epoll)\n");
    sync_bounds(&input, &gfx);

    apphost_init(&g_host);

    Screen screen;
    if (screen_load(&screen, start) != 0) {
        fprintf(stderr, "failed to load %s\n", start);
        gfx_shutdown(&gfx);
        return 1;
    }
    if (strstr(start, "home.xml"))
        apphost_refresh_launcher(&g_host, &screen);
    screen_layout(&screen, &gfx);
    input_set_dialer_mode(&input, path_is_dialer(screen.path));

    uint64_t last_tick = 0;
    while (g_run) {
        if (gfx_poll_desktop(&gfx))
            break;

        if (gfx.resized) {
            screen_layout(&screen, &gfx);
            sync_bounds(&input, &gfx);
            gfx.resized = 0;
        }

        uint64_t t = now_ms();
        if (t - last_tick > 200) {
            screen_tick(&screen, &gfx);
            apphost_on_tick(&g_host);
            apphost_update_softkeys(&g_host, &screen);
            char mode[24];
            input_mode_label(&input, mode, sizeof(mode));
            screen_set_mode_badge(&screen, mode);
            last_tick = t;
        }

        InputEvent ev;
        int got = input_poll(&input, &ev, 16);
        char nav[MAX_PATH];
        nav[0] = 0;

        if (got) {
            switch (ev.kind) {
            case IE_QUIT:
                g_run = 0;
                break;
            case IE_HOME:
                apphost_route_home(&g_host, &screen, &gfx, nav, sizeof(nav));
                break;
            case IE_NAV_UP:
                screen_focus_next(&screen, -1);
                if (screen.focus >= 0)
                    apphost_fire_event(&g_host, "focus", screen.widgets[screen.focus].id);
                apphost_update_softkeys(&g_host, &screen);
                break;
            case IE_NAV_DOWN:
                screen_focus_next(&screen, 1);
                if (screen.focus >= 0)
                    apphost_fire_event(&g_host, "focus", screen.widgets[screen.focus].id);
                apphost_update_softkeys(&g_host, &screen);
                break;
            case IE_NAV_LEFT:
                screen_focus_next(&screen, -1);
                apphost_update_softkeys(&g_host, &screen);
                break;
            case IE_NAV_RIGHT:
                screen_focus_next(&screen, 1);
                apphost_update_softkeys(&g_host, &screen);
                break;
            case IE_OK: {
                activate_focus(&screen, &gfx, nav, sizeof(nav));
                if (!nav[0] && screen.soft_center_act[0])
                    run_soft(&screen, &gfx, 2, screen.soft_center_act, nav, sizeof(nav));
                else if (!nav[0])
                    run_soft(&screen, &gfx, 2, "ok", nav, sizeof(nav));
                break;
            }
            case IE_BACK:
                if (g_host.in_app)
                    apphost_route_pop(&g_host, &screen, &gfx, nav, sizeof(nav));
                else if (strstr(screen.path, "home.xml"))
                    str_copy(nav, sizeof(nav), "assets/screens/idle.xml");
                else if (strstr(screen.path, "idle.xml"))
                    g_run = 0;
                else
                    str_copy(nav, sizeof(nav), "assets/screens/home.xml");
                break;
            case IE_SOFT_RIGHT:
                run_soft(&screen, &gfx, 3,
                         screen.soft_right_act[0] ? screen.soft_right_act : "route:pop",
                         nav, sizeof(nav));
                break;
            case IE_SOFT_LEFT:
                run_soft(&screen, &gfx, 1,
                         screen.soft_left_act[0] ? screen.soft_left_act : "ok",
                         nav, sizeof(nav));
                break;
            case IE_BACKSPACE:
                if (screen.focus >= 0)
                    input_backspace(&screen.widgets[screen.focus]);
                else {
                    Widget *w = focused_input(&screen);
                    if (w)
                        input_backspace(w);
                }
                if (g_host.in_app && screen.focus >= 0)
                    apphost_fire_event(&g_host, "key", "backspace");
                apphost_update_softkeys(&g_host, &screen);
                break;
            case IE_CHAR:
                apply_text(&screen, &input, &ev);
                if (g_host.in_app)
                    apphost_fire_event(&g_host, "key", "char");
                break;
            case IE_CLICK: {
                int sk = screen_hit_softkey(&screen, &gfx, ev.x, ev.y);
                if (sk == 1)
                    run_soft(&screen, &gfx, 1,
                             screen.soft_left_act[0] ? screen.soft_left_act : "ok",
                             nav, sizeof(nav));
                else if (sk == 2) {
                    activate_focus(&screen, &gfx, nav, sizeof(nav));
                    if (!nav[0])
                        run_soft(&screen, &gfx, 2,
                                 screen.soft_center_act[0] ? screen.soft_center_act : "ok",
                                 nav, sizeof(nav));
                } else if (sk == 3)
                    run_soft(&screen, &gfx, 3,
                             screen.soft_right_act[0] ? screen.soft_right_act : "route:pop",
                             nav, sizeof(nav));
                else if (screen_focus_at(&screen, ev.x, ev.y) >= 0) {
                    activate_focus(&screen, &gfx, nav, sizeof(nav));
                    apphost_update_softkeys(&g_host, &screen);
                }
                break;
            }
            default:
                break;
            }
        }

        if (nav[0]) {
            if (apply_nav(&screen, &gfx, &input, nav) < 0)
                fprintf(stderr, "nav failed: %s\n", nav);
        }

        screen_render(&screen, &gfx, &input.state);
        gfx_present(&gfx);
    }

    input_shutdown(&input);
    gfx_shutdown(&gfx);
    return 0;
}
