#include "window.h"
#include "draw.h"
#include "font.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

WindowManager g_wm;

static int clampi(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static OsWindow *slot_by_id(WindowManager *wm, int id)
{
    if (!wm || id <= 0)
        return NULL;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (wm->wins[i].alive && wm->wins[i].id == id)
            return &wm->wins[i];
    }
    return NULL;
}

static OsWindow *alloc_slot(WindowManager *wm)
{
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (!wm->wins[i].alive)
            return &wm->wins[i];
    }
    return NULL;
}

static void raise_z(WindowManager *wm, OsWindow *win)
{
    if (!wm || !win)
        return;
    wm->z_top++;
    win->z = wm->z_top;
}

static void clear_focus_flags(WindowManager *wm)
{
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        wm->wins[i].focused = 0;
}

static void content_rect(const OsWindow *win, int *cx, int *cy, int *cw, int *ch)
{
    *cx = win->x;
    *cy = win->y + WM_TITLE_H;
    *cw = win->w;
    *ch = win->h - WM_TITLE_H;
    if (*ch < 1)
        *ch = 1;
}

static int btn_min_x(const OsWindow *win)
{
    return win->x + win->w - WM_BTN_W * 3;
}

static int btn_max_x(const OsWindow *win)
{
    return win->x + win->w - WM_BTN_W * 2;
}

static int btn_close_x(const OsWindow *win)
{
    return win->x + win->w - WM_BTN_W;
}

static int hit_chrome(const OsWindow *win, int x, int y)
{
    if (!win || win->state == WM_ST_MINIMIZED)
        return WM_HIT_NONE;
    if (x < win->x || y < win->y || x >= win->x + win->w || y >= win->y + win->h)
        return WM_HIT_NONE;

    int on_n = (y < win->y + WM_EDGE);
    int on_s = (y >= win->y + win->h - WM_EDGE);
    int on_w = (x < win->x + WM_EDGE);
    int on_e = (x >= win->x + win->w - WM_EDGE);

    if (win->state != WM_ST_MAXIMIZED) {
        if (on_n && on_w)
            return WM_HIT_RESIZE_NW;
        if (on_n && on_e)
            return WM_HIT_RESIZE_NE;
        if (on_s && on_w)
            return WM_HIT_RESIZE_SW;
        if (on_s && on_e)
            return WM_HIT_RESIZE_SE;
        if (on_n)
            return WM_HIT_RESIZE_N;
        if (on_s)
            return WM_HIT_RESIZE_S;
        if (on_w)
            return WM_HIT_RESIZE_W;
        if (on_e)
            return WM_HIT_RESIZE_E;
    }

    if (y < win->y + WM_TITLE_H) {
        int by0 = win->y + 2;
        int by1 = win->y + WM_TITLE_H - 2;
        if (y >= by0 && y < by1) {
            int mx = btn_min_x(win);
            int ax = btn_max_x(win);
            int cx = btn_close_x(win);
            if (x >= mx && x < mx + WM_BTN_W)
                return WM_HIT_MIN;
            if (x >= ax && x < ax + WM_BTN_W)
                return WM_HIT_MAX;
            if (x >= cx && x < cx + WM_BTN_W)
                return WM_HIT_CLOSE;
        }
        return WM_HIT_MOVE;
    }
    return WM_HIT_CLIENT;
}

void wm_init(WindowManager *wm, Screen *desktop, Gfx *g)
{
    if (!wm)
        return;
    memset(wm, 0, sizeof(*wm));
    wm->desktop = desktop;
    wm->gfx = g;
    wm->focus_id = 0;
    wm->next_id = 1;
    wm->z_top = 0;
    if (g) {
        wm->work_x = 0;
        wm->work_y = 36;
        wm->work_w = g->w;
        wm->work_h = g->h - 36 - 48;
        if (wm->work_h < 120)
            wm->work_h = g->h > 120 ? g->h - 40 : g->h;
    }
}

void wm_set_work_area(WindowManager *wm, int x, int y, int w, int h)
{
    if (!wm)
        return;
    wm->work_x = x;
    wm->work_y = y;
    wm->work_w = w > 0 ? w : 1;
    wm->work_h = h > 0 ? h : 1;
}

OsWindow *wm_find(WindowManager *wm, int id)
{
    return slot_by_id(wm, id);
}

OsWindow *wm_find_app(WindowManager *wm, const char *app_id)
{
    if (!wm || !app_id || !app_id[0])
        return NULL;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (wm->wins[i].alive && str_eq(wm->wins[i].app_id, app_id))
            return &wm->wins[i];
    }
    return NULL;
}

OsWindow *wm_focused(WindowManager *wm)
{
    if (!wm)
        return NULL;
    return slot_by_id(wm, wm->focus_id);
}

Screen *wm_active_screen(WindowManager *wm)
{
    OsWindow *w = wm_focused(wm);
    if (w && w->state != WM_ST_MINIMIZED)
        return &w->screen;
    return wm ? wm->desktop : NULL;
}

int wm_is_protected_app(const char *app_id)
{
    if (!app_id || !app_id[0])
        return 1;
    if (str_eq(app_id, "launcher") || str_eq(app_id, "init") || str_eq(app_id, "pid1"))
        return 1;
    if (str_eq(app_id, "musicd") || str_eq(app_id, "procman"))
        return 1;
    return 0;
}

void wm_layout_window(WindowManager *wm, OsWindow *win)
{
    if (!wm || !wm->gfx || !win || !win->alive)
        return;
    Gfx *g = wm->gfx;
    int cx, cy, cw, ch;
    content_rect(win, &cx, &cy, &cw, &ch);
    int ow = g->w;
    int oh = g->h;
    int ost = g->status_h;
    int osk = g->softkey_h;
    g->w = cw;
    g->h = ch;
    g->status_h = 24;
    g->softkey_h = 28;
    screen_layout(&win->screen, g);
    g->w = ow;
    g->h = oh;
    g->status_h = ost;
    g->softkey_h = osk;
}

void wm_layout_all(WindowManager *wm)
{
    if (!wm)
        return;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (wm->wins[i].alive && wm->wins[i].state != WM_ST_MINIMIZED)
            wm_layout_window(wm, &wm->wins[i]);
    }
}

static void apply_max_bounds(WindowManager *wm, OsWindow *win)
{
    win->x = wm->work_x;
    win->y = wm->work_y;
    win->w = wm->work_w;
    win->h = wm->work_h;
    if (win->w < WM_MIN_W)
        win->w = WM_MIN_W;
    if (win->h < WM_MIN_H)
        win->h = WM_MIN_H;
}

int wm_open(WindowManager *wm, const PkgApp *app, const char *script, const char *title)
{
    if (!wm || !app)
        return -1;
    OsWindow *existing = wm_find_app(wm, app->id);
    if (existing) {
        if (existing->state == WM_ST_MINIMIZED)
            existing->state = WM_ST_NORMAL;
        wm_focus(wm, existing->id);
        return existing->id;
    }
    OsWindow *win = alloc_slot(wm);
    if (!win)
        return -1;
    memset(win, 0, sizeof(*win));
    win->alive = 1;
    win->id = wm->next_id++;
    win->app = *app;
    str_copy(win->app_id, sizeof(win->app_id), app->id);
    str_copy(win->title, sizeof(win->title), title && title[0] ? title : (app->name[0] ? app->name : app->id));
    if (script)
        str_copy(win->script, sizeof(win->script), script);
    win->protected_close = wm_is_protected_app(app->id);
    route_reset(&win->stack);
    screen_init(&win->screen);

    int ww = wm->work_w * 3 / 5;
    int wh = wm->work_h * 3 / 4;
    if (ww < WM_MIN_W)
        ww = WM_MIN_W;
    if (wh < WM_MIN_H)
        wh = WM_MIN_H;
    if (ww > wm->work_w)
        ww = wm->work_w;
    if (wh > wm->work_h)
        wh = wm->work_h;
    int ox = (wm->count % 5) * 18;
    int oy = (wm->count % 5) * 18;
    win->w = ww;
    win->h = wh;
    win->x = wm->work_x + (wm->work_w - ww) / 2 + ox;
    win->y = wm->work_y + (wm->work_h - wh) / 2 + oy;
    win->x = clampi(win->x, wm->work_x, wm->work_x + wm->work_w - WM_MIN_W);
    win->y = clampi(win->y, wm->work_y, wm->work_y + wm->work_h - WM_TITLE_H);
    win->rx = win->x;
    win->ry = win->y;
    win->rw = win->w;
    win->rh = win->h;
    win->state = WM_ST_NORMAL;
    wm->count++;
    raise_z(wm, win);
    wm_focus(wm, win->id);
    return win->id;
}

int wm_focus(WindowManager *wm, int id)
{
    OsWindow *win = slot_by_id(wm, id);
    if (!wm || !win)
        return -1;
    if (win->state == WM_ST_MINIMIZED)
        win->state = WM_ST_NORMAL;
    clear_focus_flags(wm);
    win->focused = 1;
    wm->focus_id = win->id;
    raise_z(wm, win);
    return 0;
}

int wm_minimize(WindowManager *wm, int id)
{
    OsWindow *win = slot_by_id(wm, id);
    if (!win)
        return -1;
    win->state = WM_ST_MINIMIZED;
    win->focused = 0;
    if (wm->focus_id == id) {
        wm->focus_id = 0;
        int best_z = -1;
        int best = 0;
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (!wm->wins[i].alive || wm->wins[i].state == WM_ST_MINIMIZED)
                continue;
            if (wm->wins[i].z > best_z) {
                best_z = wm->wins[i].z;
                best = wm->wins[i].id;
            }
        }
        if (best)
            wm_focus(wm, best);
    }
    return 0;
}

int wm_toggle_max(WindowManager *wm, int id)
{
    OsWindow *win = slot_by_id(wm, id);
    if (!wm || !win)
        return -1;
    if (win->state == WM_ST_MAXIMIZED) {
        win->x = win->rx;
        win->y = win->ry;
        win->w = win->rw;
        win->h = win->rh;
        win->state = WM_ST_NORMAL;
    } else {
        if (win->state == WM_ST_MINIMIZED)
            win->state = WM_ST_NORMAL;
        win->rx = win->x;
        win->ry = win->y;
        win->rw = win->w;
        win->rh = win->h;
        apply_max_bounds(wm, win);
        win->state = WM_ST_MAXIMIZED;
    }
    wm_layout_window(wm, win);
    wm_focus(wm, id);
    return 0;
}

int wm_close(WindowManager *wm, int id)
{
    OsWindow *win = slot_by_id(wm, id);
    if (!wm || !win)
        return -1;
    if (win->protected_close || wm_is_protected_app(win->app_id))
        return -1;
    int was_focus = (wm->focus_id == id);
    win->alive = 0;
    memset(win, 0, sizeof(*win));
    if (wm->count > 0)
        wm->count--;
    if (was_focus) {
        wm->focus_id = 0;
        int best_z = -1;
        int best = 0;
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (!wm->wins[i].alive || wm->wins[i].state == WM_ST_MINIMIZED)
                continue;
            if (wm->wins[i].z > best_z) {
                best_z = wm->wins[i].z;
                best = wm->wins[i].id;
            }
        }
        if (best)
            wm_focus(wm, best);
    }
    return 0;
}

int wm_close_app(WindowManager *wm, const char *app_id)
{
    OsWindow *win = wm_find_app(wm, app_id);
    if (!win)
        return -1;
    return wm_close(wm, win->id);
}

static void draw_titlebar(Gfx *g, OsWindow *win)
{
    uint16_t bar = win->focused ? COL_ACCENT : COL_PANEL;
    uint16_t fg = win->focused ? COL_ON_ACCENT : COL_TEXT;
    uint16_t btn_bg = win->focused ? RGB565(194, 65, 12) : COL_BORDER;
    draw_fill(g, win->x, win->y, win->w, WM_TITLE_H, bar);
    draw_rect(g, win->x, win->y, win->w, win->h, win->focused ? COL_ACCENT : COL_BORDER);

    FontStyle st = font_style_from_id(g, FONT_UI);
    st.size_px = 12;
    int ty = win->y + (WM_TITLE_H - font_char_h_style(g, &st)) / 2;
    int max_tw = win->w - WM_BTN_W * 3 - 16;
    if (max_tw < 24)
        max_tw = 24;
    font_draw_text_style_clip(g, &st, win->x + 10, ty, max_tw, win->title, fg, bar, 1);

    int by = win->y + 4;
    int bh = WM_TITLE_H - 8;
    int mx = btn_min_x(win);
    int ax = btn_max_x(win);
    int cx = btn_close_x(win);
    draw_fill(g, mx + 2, by, WM_BTN_W - 4, bh, btn_bg);
    draw_fill(g, ax + 2, by, WM_BTN_W - 4, bh, btn_bg);
    draw_fill(g, cx + 2, by, WM_BTN_W - 4, bh, win->focused ? COL_DANGER : btn_bg);

    int midy = win->y + WM_TITLE_H / 2;
    draw_hline(g, mx + 10, midy, WM_BTN_W - 20, fg);
    draw_rect(g, ax + 10, midy - 5, WM_BTN_W - 20, 10, fg);
    draw_line(g, cx + 10, midy - 5, cx + WM_BTN_W - 10, midy + 5, COL_ON_ACCENT);
    draw_line(g, cx + WM_BTN_W - 10, midy - 5, cx + 10, midy + 5, COL_ON_ACCENT);
}

void wm_render(WindowManager *wm, Gfx *g)
{
    if (!wm || !g)
        return;
    if (str_eq(g->backend, "headless") || getenv("VILAOS_HEADLESS_UI"))
        return;
    int order[WM_MAX_WINDOWS];
    int n = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (wm->wins[i].alive && wm->wins[i].state != WM_ST_MINIMIZED)
            order[n++] = i;
    }
    for (int a = 0; a < n; a++) {
        for (int b = a + 1; b < n; b++) {
            if (wm->wins[order[b]].z < wm->wins[order[a]].z) {
                int t = order[a];
                order[a] = order[b];
                order[b] = t;
            }
        }
    }
    for (int i = 0; i < n; i++) {
        OsWindow *win = &wm->wins[order[i]];
        int cx, cy, cw, ch;
        content_rect(win, &cx, &cy, &cw, &ch);
        draw_fill(g, cx, cy, cw, ch, COL_BG);
        screen_render_layer(&win->screen, g, cx, cy);
        draw_titlebar(g, win);
        gfx_dirty(g, win->x, win->y, win->w, win->h);
    }
}

void wm_sync_taskbar(WindowManager *wm, Screen *desktop)
{
    if (!wm || !desktop)
        return;
    int slot = 0;
    for (int i = 0; i < WM_MAX_WINDOWS && slot < WM_TASK_SLOTS; i++) {
        if (!wm->wins[i].alive)
            continue;
        char wid[8];
        snprintf(wid, sizeof(wid), "tb%d", slot);
        Widget *b = screen_find(desktop, wid);
        if (!b)
            break;
        str_copy(b->text, sizeof(b->text), wm->wins[i].title);
        str_copy(b->icon, sizeof(b->icon), wm->wins[i].app.icon[0] ? wm->wins[i].app.icon : wm->wins[i].app_id);
        snprintf(b->action, sizeof(b->action), "wm:focus:%d", wm->wins[i].id);
        b->visible = 1;
        b->focusable = 1;
        if (wm->wins[i].focused && wm->wins[i].state != WM_ST_MINIMIZED)
            b->bg = COL_ACCENT;
        else
            b->bg = COL_PANEL;
        b->bg_set = 1;
        slot++;
    }
    for (; slot < WM_TASK_SLOTS; slot++) {
        char wid[8];
        snprintf(wid, sizeof(wid), "tb%d", slot);
        Widget *b = screen_find(desktop, wid);
        if (!b)
            break;
        b->visible = 0;
        b->focusable = 0;
        str_copy(b->action, sizeof(b->action), "none");
    }
}

int wm_hit_test(WindowManager *wm, int x, int y, int *out_id)
{
    if (out_id)
        *out_id = 0;
    if (!wm)
        return WM_HIT_NONE;
    int best_z = -1;
    int best_hit = WM_HIT_NONE;
    int best_id = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        OsWindow *win = &wm->wins[i];
        if (!win->alive || win->state == WM_ST_MINIMIZED)
            continue;
        int hit = hit_chrome(win, x, y);
        if (hit == WM_HIT_NONE)
            continue;
        if (win->z >= best_z) {
            best_z = win->z;
            best_hit = hit;
            best_id = win->id;
        }
    }
    if (out_id)
        *out_id = best_id;
    return best_hit;
}

static void begin_drag(WindowManager *wm, OsWindow *win, int mode, int x, int y)
{
    wm->drag_mode = mode;
    wm->drag_id = win->id;
    wm->drag_ox = x;
    wm->drag_oy = y;
    wm->drag_bx = win->x;
    wm->drag_by = win->y;
    wm->drag_bw = win->w;
    wm->drag_bh = win->h;
}

static void apply_drag(WindowManager *wm, int x, int y)
{
    OsWindow *win = slot_by_id(wm, wm->drag_id);
    if (!win || wm->drag_mode == WM_HIT_NONE)
        return;
    int dx = x - wm->drag_ox;
    int dy = y - wm->drag_oy;
    int nx = wm->drag_bx;
    int ny = wm->drag_by;
    int nw = wm->drag_bw;
    int nh = wm->drag_bh;
    int mode = wm->drag_mode;

    if (mode == WM_HIT_MOVE && win->state != WM_ST_MAXIMIZED) {
        nx = wm->drag_bx + dx;
        ny = wm->drag_by + dy;
        nx = clampi(nx, wm->work_x - win->w + 48, wm->work_x + wm->work_w - 48);
        ny = clampi(ny, wm->work_y, wm->work_y + wm->work_h - WM_TITLE_H);
    } else if (win->state != WM_ST_MAXIMIZED) {
        if (mode == WM_HIT_RESIZE_E || mode == WM_HIT_RESIZE_NE || mode == WM_HIT_RESIZE_SE)
            nw = wm->drag_bw + dx;
        if (mode == WM_HIT_RESIZE_S || mode == WM_HIT_RESIZE_SE || mode == WM_HIT_RESIZE_SW)
            nh = wm->drag_bh + dy;
        if (mode == WM_HIT_RESIZE_W || mode == WM_HIT_RESIZE_NW || mode == WM_HIT_RESIZE_SW) {
            nx = wm->drag_bx + dx;
            nw = wm->drag_bw - dx;
        }
        if (mode == WM_HIT_RESIZE_N || mode == WM_HIT_RESIZE_NW || mode == WM_HIT_RESIZE_NE) {
            ny = wm->drag_by + dy;
            nh = wm->drag_bh - dy;
        }
        if (nw < WM_MIN_W) {
            if (mode == WM_HIT_RESIZE_W || mode == WM_HIT_RESIZE_NW || mode == WM_HIT_RESIZE_SW)
                nx = wm->drag_bx + wm->drag_bw - WM_MIN_W;
            nw = WM_MIN_W;
        }
        if (nh < WM_MIN_H) {
            if (mode == WM_HIT_RESIZE_N || mode == WM_HIT_RESIZE_NW || mode == WM_HIT_RESIZE_NE)
                ny = wm->drag_by + wm->drag_bh - WM_MIN_H;
            nh = WM_MIN_H;
        }
    }
    win->x = nx;
    win->y = ny;
    win->w = nw;
    win->h = nh;
    if (mode != WM_HIT_MOVE)
        wm_layout_window(wm, win);
}

int wm_pointer(WindowManager *wm, InputMux *m, InputEvent *ev, Screen **out_screen, int *out_lx, int *out_ly)
{
    if (out_screen)
        *out_screen = wm ? wm->desktop : NULL;
    if (out_lx)
        *out_lx = ev ? ev->x : 0;
    if (out_ly)
        *out_ly = ev ? ev->y : 0;
    if (!wm || !ev)
        return 0;

    if (ev->kind == IE_TOUCH_MOVE && wm->drag_mode != WM_HIT_NONE) {
        apply_drag(wm, ev->x, ev->y);
        if (out_screen)
            *out_screen = NULL;
        return 1;
    }

    if (ev->kind == IE_TOUCH_UP && wm->drag_mode != WM_HIT_NONE) {
        int mode = wm->drag_mode;
        int id = wm->drag_id;
        wm->drag_mode = WM_HIT_NONE;
        wm->drag_id = 0;
        OsWindow *win = slot_by_id(wm, id);
        if (win && mode != WM_HIT_MOVE)
            wm_layout_window(wm, win);
        if (out_screen)
            *out_screen = NULL;
        return 1;
    }

    if (ev->kind == IE_TOUCH_DOWN || ev->kind == IE_CLICK) {
        int id = 0;
        int hit = wm_hit_test(wm, ev->x, ev->y, &id);
        if (hit == WM_HIT_NONE) {
            if (wm->focus_id)
                clear_focus_flags(wm);
            wm->focus_id = 0;
            if (out_screen)
                *out_screen = wm->desktop;
            return 0;
        }
        OsWindow *win = slot_by_id(wm, id);
        if (!win)
            return 0;
        wm_focus(wm, id);
        if (hit == WM_HIT_CLIENT) {
            int cx, cy, cw, ch;
            content_rect(win, &cx, &cy, &cw, &ch);
            if (out_screen)
                *out_screen = &win->screen;
            if (out_lx)
                *out_lx = ev->x - cx;
            if (out_ly)
                *out_ly = ev->y - cy;
            (void)cw;
            (void)ch;
            (void)m;
            return 2;
        }
        if (ev->kind == IE_TOUCH_DOWN) {
            if (hit == WM_HIT_MOVE || hit == WM_HIT_RESIZE_E || hit == WM_HIT_RESIZE_S ||
                hit == WM_HIT_RESIZE_SE || hit == WM_HIT_RESIZE_W || hit == WM_HIT_RESIZE_N ||
                hit == WM_HIT_RESIZE_NW || hit == WM_HIT_RESIZE_NE || hit == WM_HIT_RESIZE_SW) {
                begin_drag(wm, win, hit, ev->x, ev->y);
                if (out_screen)
                    *out_screen = NULL;
                return 1;
            }
        }
        if (ev->kind == IE_CLICK || ev->kind == IE_TOUCH_UP) {
            if (hit == WM_HIT_MIN) {
                wm_minimize(wm, id);
                return 1;
            }
            if (hit == WM_HIT_MAX) {
                wm_toggle_max(wm, id);
                return 1;
            }
            if (hit == WM_HIT_CLOSE) {
                wm_close(wm, id);
                return 1;
            }
        }
        if (out_screen)
            *out_screen = NULL;
        return 1;
    }

    if (ev->kind == IE_TOUCH_UP) {
        int id = 0;
        int hit = wm_hit_test(wm, ev->x, ev->y, &id);
        OsWindow *win = slot_by_id(wm, id);
        if (hit == WM_HIT_MIN && win) {
            wm_minimize(wm, id);
            return 1;
        }
        if (hit == WM_HIT_MAX && win) {
            wm_toggle_max(wm, id);
            return 1;
        }
        if (hit == WM_HIT_CLOSE && win) {
            wm_close(wm, id);
            return 1;
        }
        if (hit == WM_HIT_CLIENT && win) {
            int cx, cy, cw, ch;
            content_rect(win, &cx, &cy, &cw, &ch);
            if (out_screen)
                *out_screen = &win->screen;
            if (out_lx)
                *out_lx = ev->x - cx;
            if (out_ly)
                *out_ly = ev->y - cy;
            (void)cw;
            (void)ch;
            return 2;
        }
    }

    if (ev->kind == IE_TOUCH_MOVE) {
        OsWindow *win = wm_focused(wm);
        if (win && win->state != WM_ST_MINIMIZED) {
            int cx, cy, cw, ch;
            content_rect(win, &cx, &cy, &cw, &ch);
            if (ev->x >= cx && ev->y >= cy && ev->x < cx + cw && ev->y < cy + ch) {
                if (out_screen)
                    *out_screen = &win->screen;
                if (out_lx)
                    *out_lx = ev->x - cx;
                if (out_ly)
                    *out_ly = ev->y - cy;
                return 2;
            }
        }
    }
    return 0;
}

int wm_handle_action(WindowManager *wm, const char *action)
{
    if (!wm || !action)
        return 0;
    if (str_starts(action, "wm:focus:")) {
        int id = atoi(action + 9);
        return wm_focus(wm, id) == 0 ? 1 : -1;
    }
    if (str_starts(action, "wm:min:")) {
        int id = atoi(action + 7);
        return wm_minimize(wm, id) == 0 ? 1 : -1;
    }
    if (str_starts(action, "wm:max:")) {
        int id = atoi(action + 7);
        return wm_toggle_max(wm, id) == 0 ? 1 : -1;
    }
    if (str_starts(action, "wm:close:")) {
        int id = atoi(action + 9);
        return wm_close(wm, id) == 0 ? 1 : -1;
    }
    if (str_eq(action, "wm:show_desktop")) {
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (wm->wins[i].alive)
                wm_minimize(wm, wm->wins[i].id);
        }
        return 1;
    }
    return 0;
}
