#include "types.h"
#include "util.h"
#include "fb.h"
#include "draw.h"
#include "font.h"
#include "icon.h"
#include "screen.h"
#include "input.h"
#include "apprun.h"
#include "pkg.h"
#include "syspower.h"
#include "procman.h"
#include "touch.h"
#include "vapi.h"
#include "compositor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static volatile int g_run = 1;
static AppHost g_host;
static DisplayCompositor g_comp;
static int g_pid1;
static char g_pwr_ret_app[MAX_ID];
static char g_pwr_ret_path[MAX_PATH];

static int running_as_pid1(void)
{
#ifdef _WIN32
    return 0;
#else
    return getpid() == 1;
#endif
}

static void request_stop(void)
{
    if (!g_pid1)
        g_run = 0;
}

static void on_signal(int sig)
{
    (void)sig;
}

static void hang_forever(void)
{
#ifndef _WIN32
    for (;;)
        sleep(5);
#else
    for (;;)
        ;
#endif
}

static void boot_sleep_ms(unsigned ms)
{
#ifndef _WIN32
    usleep(ms * 1000u);
#else
    (void)ms;
#endif
}

static void boot_set_stage(Screen *s, Gfx *g, InputMux *input, const char *status, int pct)
{
    Widget *st = screen_find(s, "boot_status");
    Widget *bar = screen_find(s, "boot_bar");
    screen_layout(s, g);
    if (st && status)
        str_copy(st->text, sizeof(st->text), status);
    if (bar)
        bar->value = clamp_i(pct, 0, 100);
    screen_render(s, g, &input->state);
    gfx_present(g);
}

static void remember_power_return(const Screen *s)
{
    g_pwr_ret_app[0] = 0;
    g_pwr_ret_path[0] = 0;
    if (g_host.in_app && g_host.current.id[0])
        str_copy(g_pwr_ret_app, sizeof(g_pwr_ret_app), g_host.current.id);
    if (s && s->path[0] && !strstr(s->path, "power.xml"))
        str_copy(g_pwr_ret_path, sizeof(g_pwr_ret_path), s->path);
}

static int apply_power_result(int rc, Screen *s, Gfx *g, InputMux *input)
{
    if (rc == 30) {
        syspower_off(s, g, input);
        return 1;
    }
    if (rc == 31) {
        syspower_restart(s, g, input);
        return 1;
    }
    return 0;
}

static int activate_focus(Screen *s, Gfx *g, char *nav, size_t nav_sz)
{
    if (s->focus < 0)
        return 0;
    Widget *w = &s->widgets[s->focus];
    if (w->action[0] && str_starts(w->action, "key:")) {
        return apphost_handle_action(&g_host, s, g, w->action, nav, nav_sz);
    }
    if (g_host.in_app) {
        apphost_fire_event(&g_host, "click", w->id);
        char taken[MAX_PATH];
        if (vsl_consume_nav(&g_host.vsl, taken, sizeof(taken)))
            return apphost_handle_action(&g_host, s, g, taken, nav, nav_sz);
    }
    if (w->action[0] && !str_eq(w->action, "none"))
        return apphost_handle_action(&g_host, s, g, w->action, nav, nav_sz);
    return 0;
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
    Widget *msg_input = screen_find(s, "msg_input");
    if (msg_input)
        return msg_input;
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
        if (apply_power_result(rc, s, g, input))
            return 1;
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
    if (strstr(nav, "power.xml"))
        remember_power_return(s);
    if (screen_load(s, nav) != 0)
        return -1;
    if (strstr(nav, "home.xml")) {
        apphost_route_home(&g_host, s, g, NULL, 0);
        apphost_refresh_launcher(&g_host, s);
    } else if (strstr(nav, "idle.xml")) {
        apphost_route_home(&g_host, s, g, NULL, 0);
    } else if (strstr(nav, "power.xml")) {
        g_host.in_app = 0;
        vsl_reset(&g_host.vsl);
    }
    if (strstr(s->path, "settings.xml")) {
        Widget *about = screen_find(s, "about2");
        if (about) {
            char gfx_status[MAX_TEXT];
            gfx_format_label(g, gfx_status, sizeof(gfx_status));
            str_copy(about->text, sizeof(about->text), gfx_status);
        }
    }
    screen_layout(s, g);
    input_set_dialer_mode(input, path_is_dialer(s->path));
    nav[0] = 0;
    return 1;
}

static int soft_act_is_os(const char *act)
{
    return str_starts(act, "route:") || str_starts(act, "app:") || str_starts(act, "power") ||
           str_eq(act, "back") || str_eq(act, "home") || str_eq(act, "go_home") ||
           str_eq(act, "pop_route") || str_eq(act, "clear") || str_eq(act, "options") ||
           str_eq(act, "unlock") || str_eq(act, "power_menu") || str_eq(act, "search") ||
           str_eq(act, "shade");
}

static int run_soft(Screen *s, Gfx *g, int side, const char *act, char *nav, size_t nav_sz)
{
    if (!act || !act[0])
        return 0;
    char taken[MAX_PATH];
    int matched = 0;
    int consumed = 0;
    taken[0] = 0;
    if (g_host.in_app) {
        const char *tgt = side == 1 ? "left" : (side == 2 ? "center" : "right");
        matched = apphost_fire_event(&g_host, "soft", tgt) > 0;
        consumed = vsl_consume_nav(&g_host.vsl, taken, sizeof(taken));
    }
    if (matched && consumed && taken[0])
        return apphost_handle_action(&g_host, s, g, taken, nav, nav_sz);
    if (matched && soft_act_is_os(act))
        return apphost_handle_action(&g_host, s, g, act, nav, nav_sz);
    if (matched) {
        apphost_update_softkeys(&g_host, s);
        if (nav && nav_sz)
            nav[0] = 0;
        return 2;
    }
    return apphost_handle_action(&g_host, s, g, act, nav, nav_sz);
}

static void open_power_menu(Screen *s, char *nav, size_t nav_sz)
{
    remember_power_return(s);
    str_copy(nav, nav_sz, "assets/screens/power.xml");
}

static void cancel_power_menu(Screen *s, Gfx *g, InputMux *input, char *nav, size_t nav_sz)
{
    if (g_pwr_ret_app[0]) {
        if (apphost_open_app(&g_host, s, g, g_pwr_ret_app) == 0) {
            input_set_dialer_mode(input, path_is_dialer(s->path));
            nav[0] = 0;
            g_pwr_ret_app[0] = 0;
            return;
        }
    }
    if (g_pwr_ret_path[0] && !strstr(g_pwr_ret_path, "power.xml") &&
        !strstr(g_pwr_ret_path, "boot.xml") && !strstr(g_pwr_ret_path, "shutdown.xml")) {
        str_copy(nav, nav_sz, g_pwr_ret_path);
    } else {
        str_copy(nav, nav_sz, "app:launcher");
    }
    g_pwr_ret_app[0] = 0;
    g_pwr_ret_path[0] = 0;
}

static int run_boot_sequence(Gfx *g, InputMux *input, Screen *s)
{
    if (screen_load(s, "assets/screens/boot.xml") != 0)
        return -1;
    screen_layout(s, g);
    boot_set_stage(s, g, input, "Starting VilaOS...", 20);
    boot_sleep_ms(200);

    boot_set_stage(s, g, input, "Loading apps...", 55);
    apphost_init(&g_host);
    procman_init(&g_procman);
    boot_sleep_ms(200);

    boot_set_stage(s, g, input, "Starting shell...", 85);
    boot_sleep_ms(150);
    return 0;
}

int main(int argc, char **argv)
{
    int desktop = 0;
    int skip_boot = 0;
    const char *start = "assets/screens/idle.xml";
    const char *env_start = getenv("VILAOS_SCREEN");
    if (env_start && env_start[0]) {
        start = env_start;
        skip_boot = 1;
    }
    for (int i = 1; i < argc; i++) {
        if (str_eq(argv[i], "--desktop") || str_eq(argv[i], "-d"))
            desktop = 1;
        else if (str_eq(argv[i], "--screen") && i + 1 < argc) {
            start = argv[++i];
            skip_boot = 1;
        }
    }

    g_pid1 = running_as_pid1();
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
#ifndef _WIN32
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
#endif

    Gfx gfx;
    if (gfx_init(&gfx, desktop) != 0) {
        fprintf(stderr, "gfx_init failed\n");
        if (g_pid1)
            hang_forever();
        return 1;
    }
    {
        char gfx_status[192];
        gfx_format_label(&gfx, gfx_status, sizeof(gfx_status));
        fprintf(stderr, "gfx: %s\n", gfx_status);
    }
    icon_init(&gfx);

    vapi_init();

    InputMux input;
    if (input_init(&input) != 0)
        fprintf(stderr, "input_init failed (continuing without epoll)\n");
    sync_bounds(&input, &gfx);

    Screen screen;
    if (!skip_boot) {
        if (run_boot_sequence(&gfx, &input, &screen) != 0) {
            apphost_init(&g_host);
            if (!g_procman.ready)
                procman_init(&g_procman);
        }
        start = "app:launcher";
    } else {
        apphost_init(&g_host);
        procman_init(&g_procman);
    }

    if (str_starts(start, "app:")) {
        if (!skip_boot)
            boot_set_stage(&screen, &gfx, &input, "Starting shell...", 95);
        if (apphost_open_app(&g_host, &screen, &gfx, start + 4) != 0) {
            fprintf(stderr, "failed to open %s, falling back\n", start);
            if (screen_load(&screen, "/apps/launcher/res/layout/lock.xml") != 0 &&
                screen_load(&screen, "assets/screens/idle.xml") != 0)
                hang_forever();
            screen_layout(&screen, &gfx);
            g_host.in_app = 0;
        }
    } else if (screen_load(&screen, start) != 0) {
        fprintf(stderr, "failed to load %s\n", start);
        hang_forever();
    } else {
        if (strstr(start, "home.xml"))
            apphost_refresh_launcher(&g_host, &screen);
        screen_layout(&screen, &gfx);
    }
    input_set_dialer_mode(&input, path_is_dialer(screen.path));
    screen_render(&screen, &gfx, &input.state);
    gfx_present(&gfx);

    if (compositor_start(&g_comp, &gfx, &screen, &input) != 0) {
        fprintf(stderr, "compositor_start failed, using sync present\n");
    } else {
        compositor_request_full(&g_comp);
    }

    uint64_t last_tick = 0;
restart_loop:
    while (g_run) {
        if (gfx_poll_desktop(&gfx)) {
            if (g_pid1)
                continue;
            break;
        }

        int need_frame = 0;
        int has_comp = g_comp.running;
        if (gfx.resized) {
            if (has_comp)
                compositor_lock(&g_comp);
            screen_layout(&screen, &gfx);
            sync_bounds(&input, &gfx);
            gfx.resized = 0;
            if (has_comp) {
                compositor_unlock(&g_comp);
                compositor_request_full(&g_comp);
            }
        }

        uint64_t t = now_ms();
        if (t - last_tick > 200) {
            if (has_comp)
                compositor_lock(&g_comp);
            procman_tick(&g_procman);
            screen_tick(&screen, &gfx);
            apphost_on_tick(&g_host);
            apphost_update_softkeys(&g_host, &screen);
            char mode[24];
            input_mode_label(&input, mode, sizeof(mode));
            screen_set_mode_badge(&screen, mode);
            if (has_comp)
                compositor_unlock(&g_comp);
            need_frame = 1;
            last_tick = t;
        }

        InputEvent ev;
        int got = input_poll(&input, &ev, 8);
        if (has_comp)
            compositor_note_input(&g_comp);
        char nav[MAX_PATH];
        nav[0] = 0;
        int arc = 0;

        if (got) {
            need_frame = 1;
            int heavy = (ev.kind == IE_OK || ev.kind == IE_HOME || ev.kind == IE_BACK ||
                         ev.kind == IE_SOFT_LEFT || ev.kind == IE_SOFT_RIGHT || ev.kind == IE_POWER ||
                         ev.kind == IE_CLICK || ev.kind == IE_TOUCH_UP || ev.kind == IE_TOUCH_DOWN ||
                         ev.kind == IE_TOUCH_MOVE);
            int held = 0;
            if (has_comp && !heavy) {
                compositor_lock(&g_comp);
                held = 1;
            }
            switch (ev.kind) {
            case IE_QUIT:
                if (!g_pid1)
                    request_stop();
                break;
            case IE_POWER:
                if (!strstr(screen.path, "power.xml") && !strstr(screen.path, "boot.xml") &&
                    !strstr(screen.path, "shutdown.xml"))
                    open_power_menu(&screen, nav, sizeof(nav));
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
                arc = activate_focus(&screen, &gfx, nav, sizeof(nav));
                if (apply_power_result(arc, &screen, &gfx, &input))
                    break;
                if (!nav[0] && screen.soft_center_act[0])
                    arc = run_soft(&screen, &gfx, 2, screen.soft_center_act, nav, sizeof(nav));
                else if (!nav[0])
                    arc = run_soft(&screen, &gfx, 2, "ok", nav, sizeof(nav));
                apply_power_result(arc, &screen, &gfx, &input);
                break;
            }
            case IE_BACK:
                if (strstr(screen.path, "power.xml"))
                    cancel_power_menu(&screen, &gfx, &input, nav, sizeof(nav));
                else if (strstr(screen.path, "boot.xml") || strstr(screen.path, "shutdown.xml"))
                    nav[0] = 0;
                else if (g_host.in_app)
                    apphost_route_pop(&g_host, &screen, &gfx, nav, sizeof(nav));
                else
                    nav[0] = 0;
                break;
            case IE_SOFT_RIGHT:
                arc = run_soft(&screen, &gfx, 3,
                               screen.soft_right_act[0] ? screen.soft_right_act : "route:pop",
                               nav, sizeof(nav));
                if (apply_power_result(arc, &screen, &gfx, &input))
                    break;
                if (str_eq(screen.soft_right_act, "power_cancel") && !nav[0])
                    cancel_power_menu(&screen, &gfx, &input, nav, sizeof(nav));
                break;
            case IE_SOFT_LEFT:
                arc = run_soft(&screen, &gfx, 1,
                               screen.soft_left_act[0] ? screen.soft_left_act : "ok",
                               nav, sizeof(nav));
                apply_power_result(arc, &screen, &gfx, &input);
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
                char tap_nav[MAX_PATH];
                tap_nav[0] = 0;
                int tr = screen_handle_pointer_tap(&screen, &gfx, ev.x, ev.y, tap_nav, sizeof(tap_nav));
                if (tr >= 11 && tr <= 13) {
                    int side = tr - 10;
                    arc = run_soft(&screen, &gfx, side, tap_nav, nav, sizeof(nav));
                    if (apply_power_result(arc, &screen, &gfx, &input))
                        break;
                    if (side == 3 && str_eq(screen.soft_right_act, "power_cancel") && !nav[0])
                        cancel_power_menu(&screen, &gfx, &input, nav, sizeof(nav));
                    else if (side == 3 && !nav[0] && !g_host.in_app) {
                        nav[0] = 0;
                    }
                } else if (tr == 2) {
                    arc = activate_focus(&screen, &gfx, nav, sizeof(nav));
                    apply_power_result(arc, &screen, &gfx, &input);
                    apphost_update_softkeys(&g_host, &screen);
                }
                break;
            }
            case IE_TOUCH_DOWN:
            case IE_TOUCH_MOVE:
            case IE_TOUCH_UP: {
                char tap_nav[MAX_PATH];
                tap_nav[0] = 0;
                int tr = screen_handle_touch(&screen, &gfx, &input, &ev, tap_nav, sizeof(tap_nav));
                if (ev.kind == IE_TOUCH_UP && tr >= 11 && tr <= 13) {
                    int side = tr - 10;
                    arc = run_soft(&screen, &gfx, side, tap_nav, nav, sizeof(nav));
                    if (apply_power_result(arc, &screen, &gfx, &input))
                        break;
                    if (side == 3 && str_eq(screen.soft_right_act, "power_cancel") && !nav[0])
                        cancel_power_menu(&screen, &gfx, &input, nav, sizeof(nav));
                    else if (side == 3 && !nav[0] && !g_host.in_app) {
                        nav[0] = 0;
                    }
                } else if (ev.kind == IE_TOUCH_UP && tr == 2) {
                    arc = activate_focus(&screen, &gfx, nav, sizeof(nav));
                    apply_power_result(arc, &screen, &gfx, &input);
                    apphost_update_softkeys(&g_host, &screen);
                }
                break;
            }
            default:
                break;
            }
            if (held)
                compositor_unlock(&g_comp);
        }

        if (nav[0] && strstr(screen.path, "power.xml") && strstr(nav, "home.xml")) {
            cancel_power_menu(&screen, &gfx, &input, nav, sizeof(nav));
            need_frame = 1;
        }

        if (nav[0]) {
            if (apply_nav(&screen, &gfx, &input, nav) < 0)
                fprintf(stderr, "nav failed: %s\n", nav);
            need_frame = 1;
            if (g_comp.running)
                compositor_request_full(&g_comp);
            else {
                screen_render(&screen, &gfx, &input.state);
                gfx_present(&gfx);
            }
        } else if (need_frame) {
            if (g_comp.running)
                compositor_request(&g_comp);
            else {
                screen_render(&screen, &gfx, &input.state);
                gfx_present(&gfx);
            }
        }
    }

    compositor_stop(&g_comp);

    if (g_pid1) {
        g_run = 1;
        if (compositor_start(&g_comp, &gfx, &screen, &input) != 0)
            hang_forever();
        compositor_lock(&g_comp);
        if (apphost_route_home(&g_host, &screen, &gfx, NULL, 0) >= 0) {
            input_set_dialer_mode(&input, 0);
            compositor_unlock(&g_comp);
            compositor_request_full(&g_comp);
            last_tick = 0;
            goto restart_loop;
        }
        compositor_unlock(&g_comp);
        compositor_stop(&g_comp);
        hang_forever();
    }

    input_shutdown(&input);
    procman_shutdown(&g_procman);
    icon_shutdown();
    gfx_shutdown(&gfx);
    return 0;
}
