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

static WidgetType tag_type(const char *tag)
{
    if (str_eq(tag, "screen"))
        return W_SCREEN;
    if (str_eq(tag, "vbox"))
        return W_VBOX;
    if (str_eq(tag, "hbox"))
        return W_HBOX;
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

static void layout_box(Screen *s, Gfx *g, Widget *box, int vertical)
{
    int pad = g->pad;
    int gap = g->pad;
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
                    rh = font_char_h_scaled(g, FONT_8X12) + 4;
                else if (c->type == W_BUTTON || c->type == W_INPUT)
                    rh = g->item_h;
                else if (c->type == W_LIST)
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
    int s = clamp_i(g->font_scale, 1, 3);
    draw_signal(g, g->pad, (w->h - 12 * s) / 2, 3);
    draw_battery(g, g->w - g->pad - 22 * s, (w->h - 10 * s) / 2, 78);

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
    int ty = w->y + (w->h - font_char_h_scaled(g, FONT_8X12)) / 2;
    int side = g->w / 3;
    font_draw_text_clip(g, FONT_8X12, g->pad, ty, side - g->pad, s->soft_left, COL_ACCENT, COL_SOFTKEY, 1);
    int cw = font_text_w_scaled(g, FONT_8X12, s->soft_center);
    font_draw_text(g, FONT_8X12, (g->w - cw) / 2, ty, s->soft_center, COL_TEXT, COL_SOFTKEY, 1);
    int rw = font_text_w_scaled(g, FONT_8X12, s->soft_right);
    font_draw_text(g, FONT_8X12, g->w - rw - g->pad, ty, s->soft_right, COL_ACCENT, COL_SOFTKEY, 1);
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
    case W_LABEL:
        font_draw_text_clip(g, FONT_8X12, w->x, w->y + (w->h - fh) / 2, w->w > 0 ? w->w : g->w, w->text, w->fg, COL_BG, 0);
        break;
    case W_BUTTON: {
        uint16_t bg = w->focused ? w->focus_bg : w->bg;
        draw_round_rect(g, w->x, w->y, w->w, w->h, radius, bg, 1);
        draw_round_rect(g, w->x, w->y, w->w, w->h, radius, COL_BORDER, 0);
        int tw = font_text_w_scaled(g, FONT_8X12, w->text);
        int tx = w->x + (w->w - tw) / 2;
        int ty = w->y + (w->h - fh) / 2;
        font_draw_text_clip(g, FONT_8X12, tx, ty, w->w - 4, w->text, w->fg, bg, 1);
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

void screen_tick(Screen *s, Gfx *g)
{
    if (!s)
        return;
    Widget *info = screen_find(s, "meminfo");
    if (info) {
        long total = 0, freeb = 0, avail = 0;
        read_meminfo_kb(&total, &freeb, &avail);
        uint64_t up = read_uptime_ms();
        unsigned sec = (unsigned)(up / 1000ull);
        unsigned h = sec / 3600;
        unsigned m = (sec / 60) % 60;
        unsigned ss = sec % 60;
        snprintf(info->text, sizeof(info->text),
                 "RAM %ld/%ld KB  up %u:%02u:%02u", avail, total, h, m, ss);
    }
    Widget *rss = screen_find(s, "rssinfo");
    if (rss) {
        FILE *f = fopen("/proc/self/status", "r");
        long vmrss = 0;
        if (f) {
            char line[128];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "VmRSS:", 6) == 0) {
                    sscanf(line + 6, "%ld", &vmrss);
                    break;
                }
            }
            fclose(f);
        }
        snprintf(rss->text, sizeof(rss->text), "UI RSS: %ld KB (limit 4096)", vmrss);
    }
    Widget *fbinfo = screen_find(s, "fbinfo");
    if (fbinfo && g)
        gfx_format_label(g, fbinfo->text, sizeof(fbinfo->text));
}

int screen_handle_action(Screen *s, const char *action, char *out_nav, size_t out_sz)
{
    if (!action || !action[0])
        return 0;
    if (str_starts(action, "nav:")) {
        if (out_nav)
            str_copy(out_nav, out_sz, action + 4);
        return 1;
    }
    if (str_eq(action, "back")) {
        if (out_nav)
            str_copy(out_nav, out_sz, "assets/screens/home.xml");
        return 1;
    }
    if (str_eq(action, "quit") || str_eq(action, "exit"))
        return 3;
    if (str_eq(action, "clear")) {
        if (s && s->focus >= 0) {
            Widget *w = &s->widgets[s->focus];
            if (w->type == W_INPUT)
                w->text[0] = 0;
        }
        Widget *num = screen_find(s, "number");
        if (num)
            num->text[0] = 0;
        return 2;
    }
    if (str_eq(action, "call")) {
        Widget *num = screen_find(s, "number");
        if (num && num->text[0]) {
            Widget *st = screen_find(s, "callstatus");
            if (!st)
                st = screen_find(s, "hint");
            if (st)
                snprintf(st->text, sizeof(st->text), "Calling %s...", num->text);
        }
        return 2;
    }
    if (str_eq(action, "send")) {
        Widget *body = screen_find(s, "msg");
        if (!body)
            body = screen_find(s, "note");
        Widget *st = screen_find(s, "msgstatus");
        if (!st)
            st = screen_find(s, "status");
        if (st) {
            if (body && body->text[0])
                str_copy(st->text, sizeof(st->text), "Message sent");
            else
                str_copy(st->text, sizeof(st->text), "Empty message");
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
