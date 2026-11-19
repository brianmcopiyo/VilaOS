#include "screen.h"
#include "xml.h"
#include "draw.h"
#include "font.h"
#include "fb.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {
    Screen *s;
    int stack[32];
    int sp;
} BuildCtx;

typedef struct {
    uint64_t call_start;
    int call_muted;
    int call_speaker;
    char call_number[MAX_TEXT];
    char call_name[MAX_TEXT];
    double calc_acc;
    char calc_op;
    int calc_fresh;
    char calc_buf[MAX_TEXT];
    uint64_t sw_base;
    uint64_t sw_acc;
    int sw_running;
    uint64_t tm_deadline;
    int tm_running;
    int tm_secs;
    int sound_on;
    int vibrate_on;
    int large_text;
    int profile;
} AppState;

static AppState g_app = {
    .sound_on = 1,
    .vibrate_on = 1,
    .large_text = 0,
    .profile = 0,
    .tm_secs = 60,
    .calc_fresh = 1
};

static WidgetType tag_type(const char *tag)
{
    if (str_eq(tag, "screen"))
        return W_SCREEN;
    if (str_eq(tag, "vbox"))
        return W_VBOX;
    if (str_eq(tag, "hbox"))
        return W_HBOX;
    if (str_eq(tag, "grid"))
        return W_GRID;
    if (str_eq(tag, "label"))
        return W_LABEL;
    if (str_eq(tag, "button"))
        return W_BUTTON;
    if (str_eq(tag, "input"))
        return W_INPUT;
    if (str_eq(tag, "list"))
        return W_LIST;
    if (str_eq(tag, "statusbar"))
        return W_STATUSBAR;
    if (str_eq(tag, "softkeys"))
        return W_SOFTKEYS;
    return W_LABEL;
}

static void attr_dim(const XmlStart *el, const char *name, int *val, unsigned *unit)
{
    const char *v = xml_attr(el, name);
    int out = 0;
    int u = UNIT_AUTO;
    if (v && v[0] && parse_dim(v, &out, &u) == 0) {
        *val = out;
        *unit = (unsigned)u;
        return;
    }
    *val = 0;
    *unit = UNIT_AUTO;
}

static int attr_int(const XmlStart *el, const char *name, int def)
{
    const char *v = xml_attr(el, name);
    if (!v || !v[0])
        return def;
    return atoi(v);
}

static uint16_t attr_color(const XmlStart *el, const char *name, uint16_t def)
{
    const char *v = xml_attr(el, name);
    uint16_t c;
    if (v && parse_hex_color(v, &c) == 0)
        return c;
    return def;
}

static int resolve_dim(int src, unsigned unit, int parent, int screen, int ref)
{
    if (unit == UNIT_PCT)
        return (parent * src) / 100;
    if (unit == UNIT_AUTO)
        return 0;
    return scale_i(src, screen, ref);
}

static void on_start(const XmlStart *el, void *user)
{
    BuildCtx *ctx = (BuildCtx *)user;
    Screen *s = ctx->s;
    if (s->count >= MAX_WIDGETS)
        return;

    int idx = s->count++;
    Widget *w = &s->widgets[idx];
    memset(w, 0, sizeof(*w));
    w->type = tag_type(el->tag);
    w->parent = ctx->sp > 0 ? ctx->stack[ctx->sp - 1] : -1;
    w->visible = 1;
    w->bg = COL_PANEL;
    w->fg = COL_TEXT;
    w->focus_bg = COL_FOCUS;
    w->max_len = MAX_TEXT - 1;
    w->xu = w->yu = w->wu = w->hu = UNIT_AUTO;

    const char *id = xml_attr(el, "id");
    if (id)
        str_copy(w->id, sizeof(w->id), id);

    const char *text = xml_attr(el, "text");
    if (text)
        str_copy(w->text, sizeof(w->text), text);

    const char *action = xml_attr(el, "action");
    if (action)
        str_copy(w->action, sizeof(w->action), action);

    const char *icon = xml_attr(el, "icon");
    if (icon)
        str_copy(w->icon, sizeof(w->icon), icon);

    {
        int vx = 0, vy = 0, vw = 0, vh = 0;
        unsigned ux = UNIT_AUTO, uy = UNIT_AUTO, uw = UNIT_AUTO, uh = UNIT_AUTO;
        attr_dim(el, "x", &vx, &ux);
        attr_dim(el, "y", &vy, &uy);
        attr_dim(el, "w", &vw, &uw);
        attr_dim(el, "h", &vh, &uh);
        w->sx = vx;
        w->sy = vy;
        w->sw = vw;
        w->sh = vh;
        w->xu = ux;
        w->yu = uy;
        w->wu = uw;
        w->hu = uh;
    }

    w->bg = attr_color(el, "bg", w->bg);
    w->fg = attr_color(el, "fg", w->fg);
    w->focus_bg = attr_color(el, "focus_bg", w->focus_bg);
    w->max_len = attr_int(el, "max_len", w->max_len);
    w->value = attr_int(el, "value", 0);

    if (w->type == W_BUTTON || w->type == W_INPUT)
        w->focusable = 1;

    if (w->type == W_SOFTKEYS) {
        const char *l = xml_attr(el, "left");
        const char *c = xml_attr(el, "center");
        const char *r = xml_attr(el, "right");
        const char *la = xml_attr(el, "left_action");
        const char *ca = xml_attr(el, "center_action");
        const char *ra = xml_attr(el, "right_action");
        if (l)
            str_copy(s->soft_left, sizeof(s->soft_left), l);
        if (c)
            str_copy(s->soft_center, sizeof(s->soft_center), c);
        if (r)
            str_copy(s->soft_right, sizeof(s->soft_right), r);
        if (la)
            str_copy(s->soft_left_act, sizeof(s->soft_left_act), la);
        if (ca)
            str_copy(s->soft_center_act, sizeof(s->soft_center_act), ca);
        if (ra)
            str_copy(s->soft_right_act, sizeof(s->soft_right_act), ra);
    }

    if (w->parent >= 0) {
        Widget *p = &s->widgets[w->parent];
        if (p->child_count < MAX_CHILDREN)
            p->children[p->child_count++] = idx;
    } else if (w->type == W_SCREEN) {
        s->root = idx;
    }

    if (!el->self_close && ctx->sp < 32)
        ctx->stack[ctx->sp++] = idx;
}

static void on_end(const char *tag, void *user)
{
    BuildCtx *ctx = (BuildCtx *)user;
    if (ctx->sp <= 0)
        return;
    Widget *top = &ctx->s->widgets[ctx->stack[ctx->sp - 1]];
    if (tag && tag[0] && tag_type(tag) != top->type)
        return;
    ctx->sp--;
}

static void on_text(const char *text, void *user)
{
    BuildCtx *ctx = (BuildCtx *)user;
    if (ctx->sp <= 0)
        return;
    Widget *w = &ctx->s->widgets[ctx->stack[ctx->sp - 1]];
    if (w->type == W_LABEL || w->type == W_BUTTON || w->type == W_INPUT) {
        if (!w->text[0])
            str_copy(w->text, sizeof(w->text), text);
    }
}

int screen_init(Screen *s)
{
    memset(s, 0, sizeof(*s));
    s->root = -1;
    s->focus = -1;
    return 0;
}

int screen_load(Screen *s, const char *path)
{
    screen_init(s);
    str_copy(s->path, sizeof(s->path), path);
    str_copy(s->soft_left, sizeof(s->soft_left), "Menu");
    str_copy(s->soft_center, sizeof(s->soft_center), "OK");
    str_copy(s->soft_right, sizeof(s->soft_right), "Back");

    BuildCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.s = s;

    XmlHandlers h;
    h.on_start = on_start;
    h.on_end = on_end;
    h.on_text = on_text;
    h.user = &ctx;

    if (xml_parse_file(path, &h) != 0)
        return -1;

    if (s->root < 0 && s->count > 0)
        s->root = 0;

    if (strstr(path, "call_active.xml")) {
        if (!g_app.call_start)
            g_app.call_start = now_ms();
        Widget *num = screen_find(s, "call_number");
        if (num && g_app.call_number[0])
            str_copy(num->text, sizeof(num->text), g_app.call_number);
        Widget *name = screen_find(s, "call_name");
        if (name && g_app.call_name[0])
            str_copy(name->text, sizeof(name->text), g_app.call_name);
    }

    if (strstr(path, "calculator.xml")) {
        Widget *disp = screen_find(s, "calc_display");
        if (disp) {
            if (g_app.calc_buf[0])
                str_copy(disp->text, sizeof(disp->text), g_app.calc_buf);
            else
                str_copy(disp->text, sizeof(disp->text), "0");
        }
    }

    screen_focus_first(s);
    return 0;
}

Widget *screen_widget(Screen *s, int idx)
{
    if (!s || idx < 0 || idx >= s->count)
        return NULL;
    return &s->widgets[idx];
}

Widget *screen_find(Screen *s, const char *id)
{
    if (!s || !id)
        return NULL;
    for (int i = 0; i < s->count; i++) {
        if (str_eq(s->widgets[i].id, id))
            return &s->widgets[i];
    }
    return NULL;
}

static int grid_cols_for(Gfx *g)
{
    if (g->w >= 720)
        return 5;
    if (g->w >= 480)
        return 4;
    return 3;
}

static void layout_grid(Screen *s, Gfx *g, Widget *grid)
{
    int pad = g->pad;
    int cols = grid_cols_for(g);
    int n = 0;
    for (int i = 0; i < grid->child_count; i++) {
        Widget *c = &s->widgets[grid->children[i]];
        if (c->type == W_BUTTON)
            n++;
    }
    if (n <= 0)
        return;
    int rows = (n + cols - 1) / cols;
    int gap = pad;
    int cell_w = (grid->w - pad * 2 - gap * (cols - 1)) / cols;
    int cell_h = (grid->h - pad * 2 - gap * (rows - 1)) / rows;
    if (cell_w < 48)
        cell_w = 48;
    if (cell_h < 56)
        cell_h = 56;
    int idx = 0;
    for (int i = 0; i < grid->child_count; i++) {
        Widget *c = &s->widgets[grid->children[i]];
        if (c->type != W_BUTTON)
            continue;
        int r = idx / cols;
        int col = idx % cols;
        c->w = cell_w;
        c->h = cell_h;
        c->x = grid->x + pad + col * (cell_w + gap);
        c->y = grid->y + pad + r * (cell_h + gap);
        c->focusable = 1;
        idx++;
    }
}

static void layout_box(Screen *s, Gfx *g, Widget *box, int vertical)
{
    int pad = g->pad;
    int gap = g->pad / 2 + 2;
    int cx = box->x + pad;
    int cy = box->y + pad;
    int inner_w = box->w - pad * 2;
    int inner_h = box->h - pad * 2;
    if (inner_w < 8)
        inner_w = 8;
    if (inner_h < 8)
        inner_h = 8;

    int flex_count = 0;
    int fixed = 0;
    for (int i = 0; i < box->child_count; i++) {
        Widget *c = &s->widgets[box->children[i]];
        if (c->type == W_SOFTKEYS || c->type == W_STATUSBAR)
            continue;
        int want = vertical ? resolve_dim(c->sh, c->hu, box->h, g->h, REF_H)
                            : resolve_dim(c->sw, c->wu, box->w, g->w, REF_W);
        if (want <= 0)
            flex_count++;
        else
            fixed += want + gap;
    }
    int flex_space = vertical ? inner_h : inner_w;
    flex_space -= fixed;
    if (flex_space < 0)
        flex_space = 0;
    int flex_each = flex_count > 0 ? flex_space / flex_count : 0;

    for (int i = 0; i < box->child_count; i++) {
        Widget *c = &s->widgets[box->children[i]];
        if (c->type == W_SOFTKEYS || c->type == W_STATUSBAR)
            continue;

        int rw = resolve_dim(c->sw, c->wu, box->w, g->w, REF_W);
        int rh = resolve_dim(c->sh, c->hu, box->h, g->h, REF_H);
        int rx = resolve_dim(c->sx, c->xu, box->w, g->w, REF_W);
        int ry = resolve_dim(c->sy, c->yu, box->h, g->h, REF_H);

        if (vertical) {
            if (rw <= 0)
                rw = inner_w;
            if (rh <= 0) {
                if (c->type == W_LABEL)
                    rh = font_char_h_scaled(g, FONT_8X12) + 6;
                else if (c->type == W_BUTTON || c->type == W_INPUT)
                    rh = g->item_h;
                else if (c->type == W_LIST || c->type == W_GRID || c->type == W_VBOX || c->type == W_HBOX)
                    rh = flex_each > 0 ? flex_each : inner_h / 2;
                else
                    rh = flex_each > 0 ? flex_each : g->item_h;
            }
            if (c->xu == UNIT_AUTO && c->yu == UNIT_AUTO) {
                c->x = box->x + pad;
                c->y = cy;
                cy += rh + gap;
            } else {
                c->x = box->x + (c->xu == UNIT_AUTO ? pad : rx);
                c->y = box->y + (c->yu == UNIT_AUTO ? pad : ry);
            }
            c->w = rw;
            c->h = rh;
        } else {
            if (rh <= 0)
                rh = inner_h;
            if (rw <= 0)
                rw = flex_each > 0 ? flex_each : g->item_h * 2;
            if (c->xu == UNIT_AUTO && c->yu == UNIT_AUTO) {
                c->x = cx;
                c->y = box->y + pad;
                cx += rw + gap;
            } else {
                c->x = box->x + (c->xu == UNIT_AUTO ? pad : rx);
                c->y = box->y + (c->yu == UNIT_AUTO ? pad : ry);
            }
            c->w = rw;
            c->h = rh;
        }

        if (c->type == W_VBOX)
            layout_box(s, g, c, 1);
        else if (c->type == W_HBOX)
            layout_box(s, g, c, 0);
        else if (c->type == W_GRID)
            layout_grid(s, g, c);
        else if (c->type == W_LIST) {
            int item_h = g->item_h;
            int iy = c->y + 2;
            for (int j = 0; j < c->child_count; j++) {
                Widget *item = &s->widgets[c->children[j]];
                item->x = c->x + 2;
                item->y = iy;
                item->w = c->w - 4;
                item->h = item_h;
                item->focusable = 1;
                iy += item_h + 2;
            }
        }
    }
}

void screen_layout(Screen *s, Gfx *g)
{
    if (!s || !g || s->root < 0 || g->w <= 0 || g->h <= 0)
        return;

    Widget *root = &s->widgets[s->root];
    root->x = 0;
    root->y = 0;
    root->w = g->w;
    root->h = g->h;

    for (int i = 0; i < s->count; i++) {
        Widget *w = &s->widgets[i];
        if (w->type == W_STATUSBAR) {
            w->x = 0;
            w->y = 0;
            w->w = g->w;
            w->h = g->status_h;
        } else if (w->type == W_SOFTKEYS) {
            w->x = 0;
            w->y = g->h - g->softkey_h;
            w->w = g->w;
            w->h = g->softkey_h;
        }
    }

    for (int i = 0; i < root->child_count; i++) {
        Widget *c = &s->widgets[root->children[i]];
        if (c->type == W_STATUSBAR || c->type == W_SOFTKEYS)
            continue;

        int rw = resolve_dim(c->sw, c->wu, g->w, g->w, REF_W);
        int rh = resolve_dim(c->sh, c->hu, g->h, g->h, REF_H);
        int rx = resolve_dim(c->sx, c->xu, g->w, g->w, REF_W);
        int ry = resolve_dim(c->sy, c->yu, g->h, g->h, REF_H);

        if (c->wu == UNIT_AUTO || rw <= 0)
            rw = g->w;
        if (c->hu == UNIT_AUTO || rh <= 0)
            rh = g->h - g->status_h - g->softkey_h;
        if (c->xu == UNIT_AUTO)
            rx = 0;
        if (c->yu == UNIT_AUTO)
            ry = g->status_h;

        if (ry < g->status_h)
            ry = g->status_h;
        if (ry + rh > g->h - g->softkey_h)
            rh = g->h - g->softkey_h - ry;

        c->x = rx;
        c->y = ry;
        c->w = rw;
        c->h = rh;

        if (c->type == W_VBOX)
            layout_box(s, g, c, 1);
        else if (c->type == W_HBOX)
            layout_box(s, g, c, 0);
        else if (c->type == W_GRID)
            layout_grid(s, g, c);
        else if (c->type == W_LIST) {
            int item_h = g->item_h;
            int iy = c->y + 2;
            for (int j = 0; j < c->child_count; j++) {
                Widget *item = &s->widgets[c->children[j]];
                item->x = c->x + 2;
                item->y = iy;
                item->w = c->w - 4;
                item->h = item_h;
                item->focusable = 1;
                iy += item_h + 2;
            }
        }
    }
}

void screen_set_focus(Screen *s, int idx)
{
    if (!s)
        return;
    if (s->focus >= 0 && s->focus < s->count)
        s->widgets[s->focus].focused = 0;
    s->focus = idx;
    if (idx >= 0 && idx < s->count)
        s->widgets[idx].focused = 1;
}

int screen_focus_first(Screen *s)
{
    if (!s)
        return -1;
    for (int i = 0; i < s->count; i++) {
        if (s->widgets[i].focusable && s->widgets[i].visible) {
            screen_set_focus(s, i);
            return i;
        }
    }
    screen_set_focus(s, -1);
    return -1;
}

int screen_focus_next(Screen *s, int dir)
{
    if (!s || s->count == 0)
        return -1;
    int start = s->focus >= 0 ? s->focus : -1;
    int n = s->count;
    for (int step = 1; step <= n; step++) {
        int i = (start + dir * step + n * 8) % n;
        if (s->widgets[i].focusable && s->widgets[i].visible) {
            screen_set_focus(s, i);
            Widget *p = s->widgets[i].parent >= 0 ? &s->widgets[s->widgets[i].parent] : NULL;
            if (p && p->type == W_LIST) {
                for (int k = 0; k < p->child_count; k++) {
                    if (p->children[k] == i)
                        p->list_sel = k;
                }
            }
            return i;
        }
    }
    return s->focus;
}

int screen_focus_at(Screen *s, int x, int y)
{
    if (!s)
        return -1;
    for (int i = s->count - 1; i >= 0; i--) {
        Widget *w = &s->widgets[i];
        if (!w->focusable || !w->visible)
            continue;
        Rect r = {w->x, w->y, w->w, w->h};
        if (rect_contains(&r, x, y)) {
            screen_set_focus(s, i);
            Widget *p = w->parent >= 0 ? &s->widgets[w->parent] : NULL;
            if (p && p->type == W_LIST) {
                for (int k = 0; k < p->child_count; k++) {
                    if (p->children[k] == i)
                        p->list_sel = k;
                }
            }
            return i;
        }
    }
    return -1;
}

int screen_hit_softkey(const Screen *s, const Gfx *g, int x, int y)
{
    (void)s;
    if (!g || y < g->h - g->softkey_h)
        return 0;
    int third = g->w / 3;
    if (x < third)
        return 1;
    if (x < third * 2)
        return 2;
    return 3;
}

static void draw_signal(Gfx *g, int x, int y, int bars)
{
    int unit = clamp_i(g->font_scale, 1, 3);
    for (int i = 0; i < 4; i++) {
        int h = (3 + i * 3) * unit;
        uint16_t c = i < bars ? COL_OK : COL_DIM;
        draw_fill(g, x + i * 5 * unit, y + 12 * unit - h, 3 * unit, h, c);
    }
}

static void draw_battery(Gfx *g, int x, int y, int pct)
{
    int s = clamp_i(g->font_scale, 1, 3);
    draw_rect(g, x, y, 18 * s, 10 * s, COL_TEXT);
    draw_fill(g, x + 18 * s, y + 3 * s, 2 * s, 4 * s, COL_TEXT);
    int fill = clamp_i(pct, 0, 100) * 14 * s / 100;
    uint16_t c = pct < 20 ? COL_WARN : COL_OK;
    draw_fill(g, x + 2 * s, y + 2 * s, fill, 6 * s, c);
}

static void draw_statusbar(Gfx *g, Widget *w)
{
    draw_fill(g, w->x, w->y, w->w, w->h, COL_SOFTKEY);
    draw_hline(g, 0, w->h - 1, g->w, COL_BORDER);
    int s = clamp_i(g->font_scale, 1, 3);
    draw_signal(g, g->pad, (w->h - 12 * s) / 2, 4);
    draw_battery(g, g->w - g->pad - 22 * s, (w->h - 10 * s) / 2, 86);

    time_t t = time(NULL);
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char clock[16];
    snprintf(clock, sizeof(clock), "%02d:%02d", tm_buf.tm_hour, tm_buf.tm_min);
    int tw = font_text_w_scaled(g, FONT_8X12, clock);
    int ty = (w->h - font_char_h_scaled(g, FONT_8X12)) / 2;
    font_draw_text(g, FONT_8X12, (g->w - tw) / 2, ty, clock, COL_TEXT, COL_SOFTKEY, 1);
}

static void draw_softkeys(Gfx *g, Screen *s, Widget *w)
{
    draw_fill(g, w->x, w->y, w->w, w->h, COL_SOFTKEY);
    draw_hline(g, 0, w->y, g->w, COL_BORDER);
    int third = g->w / 3;
    int ty = w->y + (w->h - font_char_h_scaled(g, FONT_8X12)) / 2;
    font_draw_text_clip(g, FONT_8X12, g->pad, ty, third - g->pad * 2, s->soft_left, COL_ACCENT, COL_SOFTKEY, 1);
    int cw = font_text_w_scaled(g, FONT_8X12, s->soft_center);
    font_draw_text(g, FONT_8X12, (g->w - cw) / 2, ty, s->soft_center, COL_TEXT, COL_SOFTKEY, 1);
    int rw = font_text_w_scaled(g, FONT_8X12, s->soft_right);
    font_draw_text(g, FONT_8X12, g->w - rw - g->pad, ty, s->soft_right, COL_ACCENT, COL_SOFTKEY, 1);
    (void)third;
}

static void draw_app_icon(Gfx *g, int cx, int cy, int size, const char *icon, uint16_t fg)
{
    int s = size / 8;
    if (s < 2)
        s = 2;
    int x = cx - size / 2;
    int y = cy - size / 2;
    if (str_eq(icon, "dialer")) {
        draw_round_rect(g, x + s, y + s, size - 2 * s, size - 2 * s, s, fg, 0);
        draw_fill(g, cx - s, y + 2 * s, 2 * s, size - 4 * s, fg);
        draw_fill(g, x + 2 * s, cy - s, size - 4 * s, 2 * s, fg);
    } else if (str_eq(icon, "messages")) {
        draw_round_rect(g, x + s, y + 2 * s, size - 2 * s, size / 2, s, fg, 0);
        draw_line(g, x + 2 * s, y + size / 2 + s, cx, y + size - 2 * s, fg);
        draw_line(g, cx, y + size - 2 * s, x + size - 2 * s, y + size / 2 + s, fg);
    } else if (str_eq(icon, "contacts")) {
        draw_round_rect(g, cx - 2 * s, y + 2 * s, 4 * s, 4 * s, 2 * s, fg, 1);
        draw_round_rect(g, x + 2 * s, cy + s, size - 4 * s, size / 3, s, fg, 1);
    } else if (str_eq(icon, "notes")) {
        draw_rect(g, x + 2 * s, y + s, size - 4 * s, size - 2 * s, fg);
        draw_hline(g, x + 3 * s, y + 3 * s, size - 6 * s, fg);
        draw_hline(g, x + 3 * s, y + 5 * s, size - 6 * s, fg);
        draw_hline(g, x + 3 * s, y + 7 * s, size / 2, fg);
    } else if (str_eq(icon, "calculator")) {
        draw_rect(g, x + s, y + s, size - 2 * s, size - 2 * s, fg);
        draw_fill(g, x + 2 * s, y + 2 * s, size - 4 * s, 2 * s, fg);
        for (int r = 0; r < 2; r++)
            for (int c = 0; c < 3; c++)
                draw_fill(g, x + 2 * s + c * 2 * s, y + 5 * s + r * 2 * s, s, s, fg);
    } else if (str_eq(icon, "clock")) {
        draw_round_rect(g, x + s, y + s, size - 2 * s, size - 2 * s, size / 2, fg, 0);
        draw_line(g, cx, cy, cx, y + 2 * s, fg);
        draw_line(g, cx, cy, cx + 2 * s, cy + s, fg);
    } else if (str_eq(icon, "files")) {
        draw_rect(g, x + 2 * s, y + 2 * s, size - 4 * s, size - 3 * s, fg);
        draw_fill(g, x + 2 * s, y + s, size / 3, 2 * s, fg);
    } else if (str_eq(icon, "settings")) {
        draw_round_rect(g, cx - 2 * s, cy - 2 * s, 4 * s, 4 * s, s, fg, 1);
        for (int a = 0; a < 4; a++) {
            int dx = (a == 0) ? 0 : (a == 1) ? 3 * s : (a == 2) ? 0 : -3 * s;
            int dy = (a == 0) ? -3 * s : (a == 1) ? 0 : (a == 2) ? 3 * s : 0;
            draw_fill(g, cx + dx - s / 2, cy + dy - s / 2, s, s, fg);
        }
    } else {
        draw_round_rect(g, x + s, y + s, size - 2 * s, size - 2 * s, s, fg, 0);
    }
}

static void draw_widget(Screen *s, Gfx *g, Widget *w)
{
    if (!w->visible)
        return;

    int radius = clamp_i(4 * g->font_scale, 3, 10);
    int fh = font_char_h_scaled(g, FONT_8X12);

    switch (w->type) {
    case W_STATUSBAR:
        draw_statusbar(g, w);
        break;
    case W_SOFTKEYS:
        draw_softkeys(g, s, w);
        break;
    case W_LABEL: {
        FontId fid = (w->value > 0 || strstr(w->id, "clock_big") || strstr(w->id, "idle_clock")) ? FONT_8X16 : FONT_8X12;
        int lh = font_char_h_scaled(g, fid);
        int ty = w->y + (w->h - lh) / 2;
        if (w->h > lh * 2)
            ty = w->y + g->pad;
        if (str_starts(w->id, "pill") || w->value == 2) {
            int tw = font_text_w_scaled(g, FONT_8X12, w->text) + g->pad * 2;
            if (tw > w->w)
                tw = w->w;
            draw_round_rect(g, w->x, w->y + (w->h - fh - 6) / 2, tw, fh + 6, radius, COL_PILL, 1);
            font_draw_text_clip(g, FONT_8X12, w->x + g->pad, w->y + (w->h - fh) / 2, tw - g->pad, w->text, COL_TEXT, COL_PILL, 1);
        } else {
            font_draw_text_clip(g, fid, w->x, ty, w->w > 0 ? w->w : g->w, w->text, w->fg, COL_BG, 0);
        }
        break;
    }
    case W_BUTTON: {
        uint16_t bg = w->focused ? w->focus_bg : w->bg;
        draw_round_rect(g, w->x, w->y, w->w, w->h, radius, bg, 1);
        draw_round_rect(g, w->x, w->y, w->w, w->h, radius, w->focused ? COL_ACCENT : COL_BORDER, 0);
        if (w->icon[0]) {
            int icon_size = clamp_i(w->h / 2, 16, 40);
            draw_app_icon(g, w->x + w->w / 2, w->y + w->h / 3, icon_size, w->icon, w->fg);
            int tw = font_text_w_scaled(g, FONT_8X12, w->text);
            int tx = w->x + (w->w - tw) / 2;
            int ty = w->y + (2 * w->h) / 3;
            font_draw_text_clip(g, FONT_8X12, tx, ty, w->w - 4, w->text, w->fg, bg, 1);
        } else {
            int tw = font_text_w_scaled(g, FONT_8X12, w->text);
            int tx = w->x + (w->w - tw) / 2;
            int ty = w->y + (w->h - fh) / 2;
            if (w->value > 0) {
                char badge[8];
                snprintf(badge, sizeof(badge), "%c", (char)w->value);
                draw_round_rect(g, w->x + g->pad, w->y + (w->h - fh - 4) / 2, fh + 8, fh + 4, radius, COL_ACCENT, 1);
                font_draw_text(g, FONT_8X12, w->x + g->pad + 4, ty, badge, COL_TEXT, COL_ACCENT, 1);
                font_draw_text_clip(g, FONT_8X12, w->x + g->pad + fh + 14, ty, w->w - fh - 20, w->text, w->fg, bg, 1);
            } else {
                font_draw_text_clip(g, FONT_8X12, tx, ty, w->w - 4, w->text, w->fg, bg, 1);
            }
        }
        break;
    }
    case W_INPUT: {
        uint16_t bg = w->focused ? w->focus_bg : w->bg;
        int ih = font_char_h_scaled(g, FONT_8X16);
        draw_round_rect(g, w->x, w->y, w->w, w->h, radius, bg, 1);
        draw_round_rect(g, w->x, w->y, w->w, w->h, radius, w->focused ? COL_ACCENT : COL_BORDER, 0);
        font_draw_text_clip(g, FONT_8X16, w->x + g->pad, w->y + (w->h - ih) / 2, w->w - g->pad * 2, w->text, w->fg, bg, 1);
        if (w->focused) {
            int tw = font_text_w_scaled(g, FONT_8X16, w->text);
            draw_vline(g, w->x + g->pad + tw + 1, w->y + 4, w->h - 8, COL_ACCENT);
        }
        break;
    }
    case W_LIST:
        draw_fill(g, w->x, w->y, w->w, w->h, COL_PANEL);
        draw_rect(g, w->x, w->y, w->w, w->h, COL_BORDER);
        for (int i = 0; i < w->child_count; i++)
            draw_widget(s, g, &s->widgets[w->children[i]]);
        break;
    case W_GRID:
        for (int i = 0; i < w->child_count; i++)
            draw_widget(s, g, &s->widgets[w->children[i]]);
        break;
    case W_VBOX:
    case W_HBOX:
    case W_SCREEN:
        for (int i = 0; i < w->child_count; i++)
            draw_widget(s, g, &s->widgets[w->children[i]]);
        break;
    default:
        break;
    }
}

static void draw_cursor(Gfx *g, const InputState *in)
{
    if (!in || !in->cursor_visible)
        return;
    int x = in->cursor_x;
    int y = in->cursor_y;
    int s = clamp_i(g->font_scale, 1, 3);
    draw_line(g, x, y, x + 8 * s, y + 8 * s, COL_TEXT);
    draw_line(g, x, y, x, y + 12 * s, COL_TEXT);
    draw_line(g, x, y + 12 * s, x + 4 * s, y + 9 * s, COL_TEXT);
    draw_line(g, x + 4 * s, y + 9 * s, x + 8 * s, y + 8 * s, COL_TEXT);
    draw_pixel(g, x + s, y + s, COL_ACCENT);
}

void screen_render(Screen *s, Gfx *g, const InputState *in)
{
    draw_fill(g, 0, 0, g->w, g->h, COL_BG);
    if (s && s->root >= 0)
        draw_widget(s, g, &s->widgets[s->root]);
    else if (s) {
        for (int i = 0; i < s->count; i++)
            draw_widget(s, g, &s->widgets[i]);
    }
    draw_cursor(g, in);
    gfx_dirty_all(g);
}

void screen_set_mode_badge(Screen *s, const char *mode)
{
    Widget *b = screen_find(s, "mode_badge");
    if (b && mode)
        str_copy(b->text, sizeof(b->text), mode);
}

static void match_contact(const char *num, char *name, size_t n)
{
    name[0] = 0;
    if (!num || !num[0])
        return;
    if (str_starts(num, "5550101") || str_starts(num, "555-0101"))
        str_copy(name, n, "Alice Chen");
    else if (str_starts(num, "5550102") || str_starts(num, "555-0102"))
        str_copy(name, n, "Bob Okonkwo");
    else if (str_starts(num, "5550103") || str_starts(num, "555-0103"))
        str_copy(name, n, "Carol Diaz");
    else if (str_starts(num, "5550104") || str_starts(num, "555-0104"))
        str_copy(name, n, "Dave Park");
}

static void calc_set_display(Screen *s)
{
    Widget *d = screen_find(s, "calc_display");
    if (!d)
        return;
    if (g_app.calc_buf[0])
        str_copy(d->text, sizeof(d->text), g_app.calc_buf);
    else
        str_copy(d->text, sizeof(d->text), "0");
}

static void calc_push_digit(Screen *s, char dig)
{
    if (g_app.calc_fresh) {
        g_app.calc_buf[0] = 0;
        g_app.calc_fresh = 0;
    }
    size_t n = strlen(g_app.calc_buf);
    if (n + 1 < sizeof(g_app.calc_buf)) {
        g_app.calc_buf[n] = dig;
        g_app.calc_buf[n + 1] = 0;
    }
    calc_set_display(s);
}

static void calc_apply_op(Screen *s, char op)
{
    double v = atof(g_app.calc_buf[0] ? g_app.calc_buf : "0");
    if (g_app.calc_op == 0) {
        g_app.calc_acc = v;
    } else {
        if (g_app.calc_op == '+')
            g_app.calc_acc += v;
        else if (g_app.calc_op == '-')
            g_app.calc_acc -= v;
        else if (g_app.calc_op == '*')
            g_app.calc_acc *= v;
        else if (g_app.calc_op == '/' && v != 0.0)
            g_app.calc_acc /= v;
    }
    if (op == '=') {
        snprintf(g_app.calc_buf, sizeof(g_app.calc_buf), "%g", g_app.calc_acc);
        g_app.calc_op = 0;
    } else {
        g_app.calc_op = op;
        g_app.calc_buf[0] = 0;
    }
    g_app.calc_fresh = 1;
    calc_set_display(s);
    Widget *expr = screen_find(s, "calc_expr");
    if (expr) {
        if (g_app.calc_op)
            snprintf(expr->text, sizeof(expr->text), "%g %c", g_app.calc_acc, g_app.calc_op);
        else
            str_copy(expr->text, sizeof(expr->text), "");
    }
}

void screen_tick(Screen *s, Gfx *g)
{
    (void)g;
    if (!s)
        return;

    time_t t = time(NULL);
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif

    Widget *idle_clock = screen_find(s, "idle_clock");
    if (idle_clock)
        snprintf(idle_clock->text, sizeof(idle_clock->text), "%02d:%02d", tm_buf.tm_hour, tm_buf.tm_min);

    Widget *idle_date = screen_find(s, "idle_date");
    if (idle_date) {
        static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        static const char *mons[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        snprintf(idle_date->text, sizeof(idle_date->text), "%s, %s %d",
                 days[tm_buf.tm_wday], mons[tm_buf.tm_mon], tm_buf.tm_mday);
    }

    Widget *match = screen_find(s, "contact_match");
    Widget *number = screen_find(s, "number");
    if (match && number) {
        char name[MAX_TEXT];
        match_contact(number->text, name, sizeof(name));
        if (name[0])
            snprintf(match->text, sizeof(match->text), "%s", name);
        else if (number->text[0])
            str_copy(match->text, sizeof(match->text), "Unknown number");
        else
            str_copy(match->text, sizeof(match->text), "Enter a number");
    }

    Widget *call_timer = screen_find(s, "call_timer");
    if (call_timer && g_app.call_start) {
        unsigned sec = (unsigned)((now_ms() - g_app.call_start) / 1000ull);
        snprintf(call_timer->text, sizeof(call_timer->text), "%02u:%02u", sec / 60, sec % 60);
    }

    Widget *mute = screen_find(s, "btn_mute");
    if (mute)
        str_copy(mute->text, sizeof(mute->text), g_app.call_muted ? "Unmute" : "Mute");
    Widget *spk = screen_find(s, "btn_speaker");
    if (spk)
        str_copy(spk->text, sizeof(spk->text), g_app.call_speaker ? "Earpiece" : "Speaker");

    Widget *sw = screen_find(s, "stopwatch_time");
    if (sw) {
        uint64_t ms = g_app.sw_acc;
        if (g_app.sw_running)
            ms += now_ms() - g_app.sw_base;
        unsigned sec = (unsigned)(ms / 1000ull);
        unsigned cs = (unsigned)((ms / 10ull) % 100ull);
        snprintf(sw->text, sizeof(sw->text), "%02u:%02u.%02u", sec / 60, sec % 60, cs);
    }

    Widget *tm = screen_find(s, "timer_time");
    if (tm) {
        int left = g_app.tm_secs;
        if (g_app.tm_running) {
            int rem = (int)((g_app.tm_deadline - now_ms()) / 1000ll);
            if (rem < 0)
                rem = 0;
            left = rem;
            if (rem == 0)
                g_app.tm_running = 0;
        }
        snprintf(tm->text, sizeof(tm->text), "%02d:%02d", left / 60, left % 60);
    }

    Widget *clock_now = screen_find(s, "clock_now");
    if (clock_now)
        snprintf(clock_now->text, sizeof(clock_now->text), "%02d:%02d:%02d", tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

    Widget *snd = screen_find(s, "toggle_sound");
    if (snd)
        snprintf(snd->text, sizeof(snd->text), "Sound: %s", g_app.sound_on ? "On" : "Off");
    Widget *vib = screen_find(s, "toggle_vibrate");
    if (vib)
        snprintf(vib->text, sizeof(vib->text), "Vibrate: %s", g_app.vibrate_on ? "On" : "Off");
    Widget *txt = screen_find(s, "toggle_text");
    if (txt)
        snprintf(txt->text, sizeof(txt->text), "Large text: %s", g_app.large_text ? "On" : "Off");
    Widget *prof = screen_find(s, "toggle_profile");
    if (prof) {
        const char *names[] = {"General", "Silent", "Meeting", "Outdoor"};
        snprintf(prof->text, sizeof(prof->text), "Profile: %s", names[g_app.profile & 3]);
    }
}

static int append_digit(Screen *s, char dig)
{
    Widget *num = screen_find(s, "number");
    if (!num)
        return 0;
    size_t n = strlen(num->text);
    if ((int)n < num->max_len && n + 1 < sizeof(num->text)) {
        num->text[n] = dig;
        num->text[n + 1] = 0;
    }
    return 2;
}

int screen_handle_action(Screen *s, const char *action, char *out_nav, size_t out_sz)
{
    if (!action || !action[0] || str_eq(action, "none"))
        return 0;
    if (str_starts(action, "nav:")) {
        if (out_nav)
            str_copy(out_nav, out_sz, action + 4);
        return 1;
    }
    if (str_eq(action, "back")) {
        if (strstr(s->path, "idle.xml")) {
            if (out_nav)
                out_nav[0] = 0;
            return 0;
        }
        if (strstr(s->path, "home.xml")) {
            if (out_nav)
                str_copy(out_nav, out_sz, "assets/screens/idle.xml");
            return 1;
        }
        if (strstr(s->path, "message_thread") || strstr(s->path, "compose")) {
            if (out_nav)
                str_copy(out_nav, out_sz, "assets/screens/messages.xml");
            return 1;
        }
        if (strstr(s->path, "contact_detail")) {
            if (out_nav)
                str_copy(out_nav, out_sz, "assets/screens/contacts.xml");
            return 1;
        }
        if (strstr(s->path, "call_active")) {
            g_app.call_start = 0;
            if (out_nav)
                str_copy(out_nav, out_sz, "assets/screens/dialer.xml");
            return 1;
        }
        if (out_nav)
            str_copy(out_nav, out_sz, "assets/screens/home.xml");
        return 1;
    }
    if (str_eq(action, "quit") || str_eq(action, "exit") || str_eq(action, "lock")) {
        if (out_nav)
            str_copy(out_nav, out_sz, "assets/screens/idle.xml");
        return 1;
    }
    if (str_eq(action, "unlock") || str_eq(action, "menu")) {
        if (out_nav)
            str_copy(out_nav, out_sz, "assets/screens/home.xml");
        return 1;
    }
    if (str_eq(action, "clear")) {
        if (s && s->focus >= 0) {
            Widget *w = &s->widgets[s->focus];
            if (w->type == W_INPUT)
                w->text[0] = 0;
        }
        Widget *num = screen_find(s, "number");
        if (num)
            num->text[0] = 0;
        Widget *match = screen_find(s, "contact_match");
        if (match)
            str_copy(match->text, sizeof(match->text), "Enter a number");
        return 2;
    }
    if (str_starts(action, "digit:")) {
        append_digit(s, action[6]);
        return 2;
    }
    if (str_eq(action, "call")) {
        Widget *num = screen_find(s, "number");
        const char *n = (num && num->text[0]) ? num->text : "";
        if (!n[0] && s->focus >= 0) {
            Widget *f = &s->widgets[s->focus];
            if (f->text[0] && strstr(f->text, "555"))
                n = f->text;
        }
        if (!n[0])
            n = "555-0101";
        str_copy(g_app.call_number, sizeof(g_app.call_number), n);
        match_contact(n, g_app.call_name, sizeof(g_app.call_name));
        if (!g_app.call_name[0])
            str_copy(g_app.call_name, sizeof(g_app.call_name), "Calling...");
        g_app.call_start = now_ms();
        g_app.call_muted = 0;
        g_app.call_speaker = 0;
        if (out_nav)
            str_copy(out_nav, out_sz, "assets/screens/call_active.xml");
        return 1;
    }
    if (str_eq(action, "end_call")) {
        g_app.call_start = 0;
        if (out_nav)
            str_copy(out_nav, out_sz, "assets/screens/dialer.xml");
        return 1;
    }
    if (str_eq(action, "mute")) {
        g_app.call_muted = !g_app.call_muted;
        return 2;
    }
    if (str_eq(action, "speaker")) {
        g_app.call_speaker = !g_app.call_speaker;
        return 2;
    }
    if (str_eq(action, "send")) {
        Widget *body = screen_find(s, "msg");
        if (!body)
            body = screen_find(s, "note");
        Widget *st = screen_find(s, "msgstatus");
        if (!st)
            st = screen_find(s, "status_line");
        if (st) {
            if (body && body->text[0])
                str_copy(st->text, sizeof(st->text), "Sent");
            else
                str_copy(st->text, sizeof(st->text), "Nothing to send");
        }
        return 2;
    }
    if (str_eq(action, "save_note")) {
        Widget *st = screen_find(s, "note_status");
        if (st)
            str_copy(st->text, sizeof(st->text), "Note saved");
        return 2;
    }
    if (str_starts(action, "calc:")) {
        const char *p = action + 5;
        if (str_eq(p, "clear")) {
            g_app.calc_buf[0] = 0;
            g_app.calc_acc = 0;
            g_app.calc_op = 0;
            g_app.calc_fresh = 1;
            calc_set_display(s);
            Widget *expr = screen_find(s, "calc_expr");
            if (expr)
                expr->text[0] = 0;
            return 2;
        }
        if (str_eq(p, "eq")) {
            calc_apply_op(s, '=');
            return 2;
        }
        if (p[0] && !p[1] && (p[0] == '+' || p[0] == '-' || p[0] == '*' || p[0] == '/')) {
            calc_apply_op(s, p[0]);
            return 2;
        }
        if (p[0] == 'd' && p[1] == ':') {
            calc_push_digit(s, p[2]);
            return 2;
        }
        if (str_eq(p, "dot")) {
            if (!strchr(g_app.calc_buf, '.'))
                calc_push_digit(s, '.');
            return 2;
        }
        return 2;
    }
    if (str_eq(action, "sw_start")) {
        if (!g_app.sw_running) {
            g_app.sw_base = now_ms();
            g_app.sw_running = 1;
        }
        return 2;
    }
    if (str_eq(action, "sw_stop")) {
        if (g_app.sw_running) {
            g_app.sw_acc += now_ms() - g_app.sw_base;
            g_app.sw_running = 0;
        }
        return 2;
    }
    if (str_eq(action, "sw_reset")) {
        g_app.sw_running = 0;
        g_app.sw_acc = 0;
        return 2;
    }
    if (str_eq(action, "tm_start")) {
        if (!g_app.tm_running) {
            g_app.tm_deadline = now_ms() + (uint64_t)g_app.tm_secs * 1000ull;
            g_app.tm_running = 1;
        }
        return 2;
    }
    if (str_eq(action, "tm_stop")) {
        if (g_app.tm_running) {
            int rem = (int)((g_app.tm_deadline - now_ms()) / 1000ll);
            if (rem < 0)
                rem = 0;
            g_app.tm_secs = rem;
            g_app.tm_running = 0;
        }
        return 2;
    }
    if (str_eq(action, "tm_reset")) {
        g_app.tm_running = 0;
        g_app.tm_secs = 60;
        return 2;
    }
    if (str_eq(action, "tm_plus")) {
        g_app.tm_secs += 30;
        return 2;
    }
    if (str_eq(action, "toggle_sound")) {
        g_app.sound_on = !g_app.sound_on;
        return 2;
    }
    if (str_eq(action, "toggle_vibrate")) {
        g_app.vibrate_on = !g_app.vibrate_on;
        return 2;
    }
    if (str_eq(action, "toggle_text")) {
        g_app.large_text = !g_app.large_text;
        return 2;
    }
    if (str_eq(action, "cycle_profile")) {
        g_app.profile = (g_app.profile + 1) & 3;
        return 2;
    }
    if (str_eq(action, "preview")) {
        Widget *preview = screen_find(s, "file_preview");
        if (preview && s->focus >= 0) {
            Widget *f = &s->widgets[s->focus];
            char tmp[MAX_TEXT];
            str_copy(tmp, sizeof(tmp), f->text);
            snprintf(preview->text, sizeof(preview->text), "Preview: %s", tmp);
        }
        return 2;
    }
    if (str_eq(action, "select") || str_eq(action, "ok")) {
        if (s && s->focus >= 0) {
            Widget *w = &s->widgets[s->focus];
            if (w->action[0])
                return screen_handle_action(s, w->action, out_nav, out_sz);
        }
        return 2;
    }
    if (out_nav)
        str_copy(out_nav, out_sz, action);
    return 1;
}
