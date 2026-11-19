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

static void activate_focus(Screen *s, char *nav, size_t nav_sz)
{
    if (s->focus < 0)
        return;
    Widget *w = &s->widgets[s->focus];
    if (g_host.in_app) {
        apphost_fire_event(&g_host, "click", w->id);
        if (vsl_has_nav(&g_host.vsl)) {
            const char *n = vsl_take_nav(&g_host.vsl);
            if (str_eq(n, "shell:back")) {
                str_copy(nav, nav_sz, "assets/screens/home.xml");
                g_host.in_app = 0;
                vsl_reset(&g_host.vsl);
            } else {
                str_copy(nav, nav_sz, n);
            }
            g_host.vsl.nav[0] = 0;
            return;
        }
    }
    if (w->action[0])
        apphost_handle_action(&g_host, s, NULL, w->action, nav, nav_sz);
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
    if (str_eq(nav, "back") || str_eq(nav, "shell:back")) {
        g_host.in_app = 0;
        vsl_reset(&g_host.vsl);
        str_copy(nav, MAX_PATH, "assets/screens/home.xml");
    }
    if (str_starts(nav, "app:")) {
        if (apphost_open_app(&g_host, s, g, nav + 4) == 0) {
            input_set_dialer_mode(input, path_is_dialer(s->path));
            nav[0] = 0;
            return 1;
        }
        return -1;
    }
    if (str_starts(nav, "/apps/") || (g_host.in_app && !str_starts(nav, "assets/"))) {
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
        g_host.in_app = 0;
        vsl_reset(&g_host.vsl);
        apphost_refresh_launcher(&g_host, s);
    } else if (strstr(nav, "idle.xml")) {
        g_host.in_app = 0;
        vsl_reset(&g_host.vsl);
    }
    screen_layout(s, g);
    input_set_dialer_mode(input, path_is_dialer(s->path));
    nav[0] = 0;
    return 1;
}

static int run_soft(Screen *s, Gfx *g, const char *act, char *nav, size_t nav_sz)
{
    if (!act || !act[0])
        return 0;
    if (g_host.in_app) {
        if (str_eq(act, s->soft_left_act) || str_eq(act, "ok"))
            apphost_fire_event(&g_host, "soft", "left");
        else if (str_eq(act, s->soft_center_act))
            apphost_fire_event(&g_host, "soft", "center");
        else if (str_eq(act, s->soft_right_act) || str_eq(act, "back"))
            apphost_fire_event(&g_host, "soft", "right");
        if (vsl_has_nav(&g_host.vsl)) {
            const char *n = vsl_take_nav(&g_host.vsl);
            str_copy(nav, nav_sz, n);
            g_host.vsl.nav[0] = 0;
            if (str_eq(nav, "shell:back"))
                str_copy(nav, nav_sz, "assets/screens/home.xml");
            return 1;
        }
    }
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
            case IE_NAV_UP:
                screen_focus_next(&screen, -1);
                if (screen.focus >= 0)
                    apphost_fire_event(&g_host, "focus", screen.widgets[screen.focus].id);
                break;
            case IE_NAV_DOWN:
                screen_focus_next(&screen, 1);
                if (screen.focus >= 0)
                    apphost_fire_event(&g_host, "focus", screen.widgets[screen.focus].id);
                break;
            case IE_NAV_LEFT:
                screen_focus_next(&screen, -1);
                break;
            case IE_NAV_RIGHT:
                screen_focus_next(&screen, 1);
                break;
            case IE_OK: {
                activate_focus(&screen, nav, sizeof(nav));
                if (!nav[0] && screen.soft_center_act[0])
                    run_soft(&screen, &gfx, screen.soft_center_act, nav, sizeof(nav));
                else if (!nav[0])
                    run_soft(&screen, &gfx, "ok", nav, sizeof(nav));
                break;
            }
            case IE_BACK:
                run_soft(&screen, &gfx, screen.soft_right_act[0] ? screen.soft_right_act : "back", nav, sizeof(nav));
                if (strstr(screen.path, "idle.xml") && !nav[0])
                    g_run = 0;
                break;
            case IE_SOFT_RIGHT:
                run_soft(&screen, &gfx, screen.soft_right_act[0] ? screen.soft_right_act : "back", nav, sizeof(nav));
                break;
            case IE_SOFT_LEFT:
                run_soft(&screen, &gfx, screen.soft_left_act[0] ? screen.soft_left_act : "ok", nav, sizeof(nav));
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
                break;
            case IE_CHAR:
                apply_text(&screen, &input, &ev);
                if (g_host.in_app)
                    apphost_fire_event(&g_host, "key", "char");
                break;
            case IE_CLICK: {
                int sk = screen_hit_softkey(&screen, &gfx, ev.x, ev.y);
                if (sk == 1)
                    run_soft(&screen, &gfx, screen.soft_left_act[0] ? screen.soft_left_act : "ok", nav, sizeof(nav));
                else if (sk == 2) {
                    activate_focus(&screen, nav, sizeof(nav));
                    if (!nav[0])
                        run_soft(&screen, &gfx, screen.soft_center_act[0] ? screen.soft_center_act : "ok", nav, sizeof(nav));
                } else if (sk == 3)
                    run_soft(&screen, &gfx, screen.soft_right_act[0] ? screen.soft_right_act : "back", nav, sizeof(nav));
                else if (screen_focus_at(&screen, ev.x, ev.y) >= 0)
                    activate_focus(&screen, nav, sizeof(nav));
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
