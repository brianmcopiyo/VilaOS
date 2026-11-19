#include "types.h"
#include "util.h"
#include "fb.h"
#include "draw.h"
#include "font.h"
#include "screen.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static volatile int g_run = 1;

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
    if (w->action[0])
        screen_handle_action(s, w->action, nav, nav_sz);
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
}

static int path_is_dialer(const char *path)
{
    return path && strstr(path, "dialer.xml") != NULL;
}

static void sync_bounds(InputMux *input, Gfx *gfx)
{
    input->state.bound_w = gfx->w;
    input->state.bound_h = gfx->h;
    input->state.cursor_x = clamp_i(input->state.cursor_x, 0, gfx->w > 0 ? gfx->w - 1 : 0);
    input->state.cursor_y = clamp_i(input->state.cursor_y, 0, gfx->h > 0 ? gfx->h - 1 : 0);
}

int main(int argc, char **argv)
{
    int desktop = 0;
    const char *start = "assets/screens/home.xml";
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

    Screen screen;
    if (screen_load(&screen, start) != 0) {
        fprintf(stderr, "failed to load %s\n", start);
        gfx_shutdown(&gfx);
        return 1;
    }
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
        if (t - last_tick > 500) {
            screen_tick(&screen, &gfx);
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
                break;
            case IE_NAV_DOWN:
                screen_focus_next(&screen, 1);
                break;
            case IE_NAV_LEFT:
                screen_focus_next(&screen, -1);
                break;
            case IE_NAV_RIGHT:
                screen_focus_next(&screen, 1);
                break;
            case IE_OK: {
                int rc = 0;
                activate_focus(&screen, nav, sizeof(nav));
                if (!nav[0] && screen.soft_center_act[0])
                    rc = screen_handle_action(&screen, screen.soft_center_act, nav, sizeof(nav));
                else if (!nav[0])
                    rc = screen_handle_action(&screen, "ok", nav, sizeof(nav));
                if (rc == 3)
                    g_run = 0;
                break;
            }
            case IE_BACK: {
                int rc = screen_handle_action(&screen, "back", nav, sizeof(nav));
                if (str_eq(screen.path, "assets/screens/home.xml") || rc == 3)
                    g_run = 0;
                break;
            }
            case IE_SOFT_RIGHT: {
                int rc;
                if (screen.soft_right_act[0])
                    rc = screen_handle_action(&screen, screen.soft_right_act, nav, sizeof(nav));
                else
                    rc = screen_handle_action(&screen, "back", nav, sizeof(nav));
                if (rc == 3)
                    g_run = 0;
                break;
            }
            case IE_SOFT_LEFT: {
                int rc;
                if (screen.soft_left_act[0])
                    rc = screen_handle_action(&screen, screen.soft_left_act, nav, sizeof(nav));
                else {
                    activate_focus(&screen, nav, sizeof(nav));
                    rc = 0;
                }
                if (rc == 3)
                    g_run = 0;
                break;
            }
            case IE_BACKSPACE:
                if (screen.focus >= 0)
                    input_backspace(&screen.widgets[screen.focus]);
                break;
            case IE_CHAR:
                apply_text(&screen, &input, &ev);
                break;
            case IE_CLICK:
                if (screen_focus_at(&screen, ev.x, ev.y) >= 0)
                    activate_focus(&screen, nav, sizeof(nav));
                break;
            default:
                break;
            }
        }

        if (nav[0]) {
            if (screen_load(&screen, nav) != 0)
                fprintf(stderr, "nav failed: %s\n", nav);
            else {
                screen_layout(&screen, &gfx);
                input_set_dialer_mode(&input, path_is_dialer(screen.path));
            }
        }

        screen_render(&screen, &gfx, &input.state);
        gfx_present(&gfx);
    }

    input_shutdown(&input);
    gfx_shutdown(&gfx);
    return 0;
}
