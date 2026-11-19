#include "touch.h"
#include "util.h"

#include <string.h>

static int widget_hit(const Widget *w, int x, int y)
{
    if (!w || !w->visible || w->w <= 0 || w->h <= 0)
        return 0;
    Rect r = {w->x, w->y, w->w, w->h};
    return rect_contains(&r, x, y);
}

int screen_find_scrollable(Screen *s, int x, int y)
{
    if (!s)
        return -1;
    for (int i = s->count - 1; i >= 0; i--) {
        Widget *w = &s->widgets[i];
        if (!w->visible)
            continue;
        if (w->type != W_LIST && w->type != W_VBOX)
            continue;
        if (widget_hit(w, x, y))
            return i;
    }
    return -1;
}

static int list_content_h(const Screen *s, const Widget *list, const Gfx *g)
{
    (void)s;
    if (!list || !g)
        return 0;
    int item_h = g->item_h + 2;
    return list->child_count * item_h + 4;
}

static int vbox_content_h(const Screen *s, const Widget *box)
{
    if (!s || !box || box->child_count <= 0)
        return box ? box->h : 0;
    int max_b = box->y;
    for (int i = 0; i < box->child_count; i++) {
        const Widget *c = &s->widgets[box->children[i]];
        int b = c->y + c->h;
        if (b > max_b)
            max_b = b;
    }
    return max_b - box->y + 4;
}

int screen_apply_scroll(Screen *s, Gfx *g, int wi, int dy)
{
    if (!s || !g || wi < 0 || wi >= s->count || dy == 0)
        return 0;
    Widget *w = &s->widgets[wi];
    int content = 0;
    if (w->type == W_LIST)
        content = list_content_h(s, w, g);
    else if (w->type == W_VBOX)
        content = vbox_content_h(s, w);
    else
        return 0;
    int max_scroll = content - w->h;
    if (max_scroll < 0)
        max_scroll = 0;
    int ns = w->scroll - dy;
    if (ns < 0)
        ns = 0;
    if (ns > max_scroll)
        ns = max_scroll;
    if (ns == w->scroll)
        return 0;
    w->scroll = ns;
    screen_layout(s, g);
    gfx_dirty(g, w->x, w->y, w->w, w->h);
    return 1;
}

static int activate_tap(Screen *s, Gfx *g, int x, int y, char *out_nav, size_t out_sz)
{
    if (out_nav && out_sz)
        out_nav[0] = 0;
    if (screen_hit_statusbar(s, g, x, y)) {
        if (out_nav)
            str_copy(out_nav, out_sz, "shade");
        return 11;
    }
    int sk = screen_hit_softkey(s, g, x, y);
    if (sk == 1) {
        if (out_nav)
            str_copy(out_nav, out_sz, s->soft_left_act[0] ? s->soft_left_act : "ok");
        return 10 + 1;
    }
    if (sk == 2) {
        if (out_nav)
            str_copy(out_nav, out_sz, s->soft_center_act[0] ? s->soft_center_act : "ok");
        return 10 + 2;
    }
    if (sk == 3) {
        if (out_nav)
            str_copy(out_nav, out_sz, s->soft_right_act[0] ? s->soft_right_act : "route:pop");
        return 10 + 3;
    }
    if (screen_focus_at(s, x, y) >= 0)
        return 2;
    return 0;
}

int screen_handle_pointer_tap(Screen *s, Gfx *g, int x, int y, char *out_nav, size_t out_sz)
{
    return activate_tap(s, g, x, y, out_nav, out_sz);
}

int screen_handle_touch(Screen *s, Gfx *g, InputMux *m, const InputEvent *ev, char *out_nav, size_t out_sz)
{
    if (!s || !g || !m || !ev)
        return 0;
    if (out_nav && out_sz)
        out_nav[0] = 0;

    int slop = input_tap_slop(m);

    if (ev->kind == IE_TOUCH_DOWN) {
        m->gesture_active = 1;
        m->gesture_scroll = 0;
        m->gesture_start_x = ev->x;
        m->gesture_start_y = ev->y;
        m->gesture_last_x = ev->x;
        m->gesture_last_y = ev->y;
        m->gesture_start_ms = now_ms();
        m->gesture_target = screen_find_scrollable(s, ev->x, ev->y);
        screen_hit_softkey(s, g, ev->x, ev->y);
        screen_focus_at(s, ev->x, ev->y);
        gfx_dirty(g, 0, 0, g->w, g->h);
        return 1;
    }

    if (ev->kind == IE_TOUCH_MOVE) {
        if (!m->gesture_active)
            return 0;
        int dx = ev->x - m->gesture_start_x;
        int dy = ev->y - m->gesture_start_y;
        int adx = dx < 0 ? -dx : dx;
        int ady = dy < 0 ? -dy : dy;
        if (!m->gesture_scroll && (adx > slop || ady > slop)) {
            if (ady >= adx && m->gesture_target >= 0)
                m->gesture_scroll = 1;
            else if (adx > ady)
                m->gesture_scroll = 2;
            else if (m->gesture_target >= 0)
                m->gesture_scroll = 1;
        }
        if (m->gesture_scroll == 1 && m->gesture_target >= 0) {
            int step = ev->y - m->gesture_last_y;
            if (screen_apply_scroll(s, g, m->gesture_target, step))
                gfx_dirty(g, 0, 0, g->w, g->h);
        } else if (!m->gesture_scroll) {
            screen_focus_at(s, ev->x, ev->y);
        }
        m->gesture_last_x = ev->x;
        m->gesture_last_y = ev->y;
        return 0;
    }

    if (ev->kind == IE_TOUCH_UP) {
        int was_scroll = m->gesture_scroll;
        int sx = m->gesture_start_x;
        int sy = m->gesture_start_y;
        m->gesture_active = 0;
        m->gesture_scroll = 0;
        m->gesture_target = -1;
        if (was_scroll)
            return 0;
        int dx = ev->x - sx;
        int dy = ev->y - sy;
        if (dx < 0)
            dx = -dx;
        if (dy < 0)
            dy = -dy;
        if (dx > slop || dy > slop)
            return 0;
        return activate_tap(s, g, sx, sy, out_nav, out_sz);
    }
    return 0;
}
