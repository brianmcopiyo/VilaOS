#include "screen.h"
#include "xml.h"
#include "xml_ui.h"
#include "draw.h"
#include "font.h"
#include "fb.h"
#include "util.h"
#include "procman.h"
#include "compose.h"
#include "contacts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static XmlUiResources g_ui_res;

#define CARRY_MAX 8

typedef struct {
    char id[MAX_ID];
    char text[MAX_TEXT];
} CarryText;

static CarryText g_carry[CARRY_MAX];
static int g_carry_n;

void screen_carry_text(const char *id, const char *text)
{
    if (!id || !id[0])
        return;
    for (int i = 0; i < g_carry_n; i++) {
        if (str_eq(g_carry[i].id, id)) {
            str_copy(g_carry[i].text, sizeof(g_carry[i].text), text ? text : "");
            return;
        }
    }
    if (g_carry_n >= CARRY_MAX)
        return;
    str_copy(g_carry[g_carry_n].id, sizeof(g_carry[g_carry_n].id), id);
    str_copy(g_carry[g_carry_n].text, sizeof(g_carry[g_carry_n].text), text ? text : "");
    g_carry_n++;
}

static void carry_apply(Screen *s)
{
    for (int i = 0; i < g_carry_n; i++) {
        Widget *w = screen_find(s, g_carry[i].id);
        if (w)
            str_copy(w->text, sizeof(w->text), g_carry[i].text);
    }
    g_carry_n = 0;
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
    if (xml_ui_load_screen(s, path, &g_ui_res) != 0)
        return -1;

    carry_apply(s);

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

void screen_layout(Screen *s, Gfx *g)
{
    xml_ui_layout(s, g);
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
        if (w->parent >= 0) {
            Widget *p = &s->widgets[w->parent];
            if (p->type == W_LIST) {
                Rect clip = {p->x, p->y, p->w, p->h};
                if (!rect_contains(&clip, x, y))
                    continue;
            }
        }
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
    if (!g)
        return 0;
    int top = g->h - g->softkey_h - 4;
    if (top < 0)
        top = 0;
    if (y < top)
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
    for (int i = 0; i < 4; i++) {
        int h = 3 + i * 3;
        uint16_t c = i < bars ? COL_OK : COL_DIM;
        draw_fill(g, x + i * 5, y + 12 - h, 3, h, c);
    }
}

static void draw_battery(Gfx *g, int x, int y, int pct)
{
    draw_rect(g, x, y, 18, 10, COL_TEXT);
    draw_fill(g, x + 18, y + 3, 2, 4, COL_TEXT);
    int fill = clamp_i(pct, 0, 100) * 14 / 100;
    uint16_t c = pct < 20 ? COL_WARN : COL_OK;
    draw_fill(g, x + 2, y + 2, fill, 6, c);
}

static void draw_statusbar(Gfx *g, Widget *w)
{
    draw_fill(g, w->x, w->y, w->w, w->h, COL_SOFTKEY);
    draw_hline(g, 0, w->y + w->h - 1, g->w, COL_BORDER);
    draw_hline(g, 0, w->h - 1, g->w, COL_BORDER);
    draw_signal(g, g->pad, (w->h - 12) / 2, 4);
    draw_battery(g, g->w - g->pad - 22, (w->h - 10) / 2, 86);

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
    if (g_procman.ready) {
        ProcService *ms = procman_find(&g_procman, "musicd");
        if (ms && ms->state == SVC_RUNNING && ms->pid > 0)
            font_draw_text(g, FONT_8X12, g->pad + 22, ty, "M", COL_ACCENT, COL_SOFTKEY, 1);
    }
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
    } else if (str_eq(icon, "music")) {
        draw_round_rect(g, cx - s, y + 2 * s, 2 * s, size - 4 * s, s / 2, fg, 1);
        draw_round_rect(g, cx + s, y + 3 * s, size / 3, size / 3, s, fg, 1);
        draw_hline(g, cx - s, y + 2 * s, size / 2, fg);
    } else {
        draw_round_rect(g, x + s, y + s, size - 2 * s, size - 2 * s, s, fg, 0);
    }
}

static FontStyle widget_font_style(Gfx *g, Widget *w, FontId fallback_id)
{
    FontStyle base = font_style_from_id(g, fallback_id);
    if (!w)
        return base;
    if (!w->font_family[0] && w->font_size <= 0 && w->font_weight <= 0)
        return base;
    FontStyle ws = font_style_from_widget(w, base.size_px);
    if (!w->font_family[0])
        ws.face = base.face;
    if (w->font_size <= 0)
        ws.size_px = base.size_px;
    if (w->font_weight <= 0)
        ws.weight = base.weight;
    return ws;
}

static int is_surface_component(const Widget *w)
{
    return str_eq(w->component, "card") || str_eq(w->component, "surface");
}

static void draw_box_decor(Gfx *g, Widget *w)
{
    if (w->w <= 0 || w->h <= 0)
        return;
    int surface = is_surface_component(w);
    int fill = w->bg_set || surface;
    if (w->type == W_SCREEN && w->bg == COL_BG)
        fill = 0;
    int bw = w->border > 0 ? clamp_i(w->border, 1, 6) : 0;
    if (!fill && bw == 0)
        return;
    int br = w->radius > 0 ? clamp_i(w->radius, 2, 24) : 0;
    if (br * 2 > w->w)
        br = w->w / 2;
    if (br * 2 > w->h)
        br = w->h / 2;
    if (fill) {
        if (br > 0)
            draw_round_rect(g, w->x, w->y, w->w, w->h, br, w->bg, 1);
        else
            draw_fill(g, w->x, w->y, w->w, w->h, w->bg);
    }
    uint16_t bc = w->border_c ? w->border_c : COL_BORDER;
    for (int i = 0; i < bw; i++) {
        int ix = w->x + i;
        int iy = w->y + i;
        int iw = w->w - 2 * i;
        int ih = w->h - 2 * i;
        if (iw <= 0 || ih <= 0)
            break;
        if (br > 0)
            draw_round_rect(g, ix, iy, iw, ih, br > i ? br - i : 0, bc, 0);
        else
            draw_rect(g, ix, iy, iw, ih, bc);
    }
}

static void clip_enter(Gfx *g, const Widget *w, Rect *saved, int *active)
{
    *active = 0;
    if (!g->composer || w->overflow == 0 || w->w <= 0 || w->h <= 0)
        return;
    Rect cur = {0, 0, g->w, g->h};
    if (g->composer->clip_depth > 0 && g->composer->clip[0].w > 0 && g->composer->clip[0].h > 0)
        cur = g->composer->clip[0];
    *saved = cur;
    Rect box = {w->x, w->y, w->w, w->h};
    rect_intersect(&box, &cur);
    if (box.w < 0)
        box.w = 0;
    if (box.h < 0)
        box.h = 0;
    compose_set_clip(g->composer, box.x, box.y, box.w, box.h);
    *active = 1;
}

static void clip_leave(Gfx *g, const Rect *saved, int active)
{
    if (active && g->composer)
        compose_set_clip(g->composer, saved->x, saved->y, saved->w, saved->h);
}

static void draw_container_children(Screen *s, Gfx *g, Widget *w);

static void draw_container(Screen *s, Gfx *g, Widget *w)
{
    draw_box_decor(g, w);
    Rect saved = {0, 0, 0, 0};
    int active = 0;
    clip_enter(g, w, &saved, &active);
    draw_container_children(s, g, w);
    clip_leave(g, &saved, active);
}

static int label_text_x(Gfx *g, const FontStyle *st, const Widget *w, const char *text, int x, int avail)
{
    if (w->text_align == 0 || avail <= 0)
        return x;
    int tw = font_text_w_style(g, st, text);
    if (tw >= avail)
        return x;
    if (w->text_align == 1)
        return x + (avail - tw) / 2;
    return x + avail - tw;
}

static const char *label_text_fit(Gfx *g, const FontStyle *st, const Widget *w, int avail, char *buf, size_t buf_sz)
{
    if (w->overflow != 3 || avail <= 0 || font_text_w_style(g, st, w->text) <= avail)
        return w->text;
    size_t n = strlen(w->text);
    if (n >= buf_sz - 4)
        n = buf_sz - 4;
    while (n > 0) {
        memcpy(buf, w->text, n);
        memcpy(buf + n, "...", 4);
        if (font_text_w_style(g, st, buf) <= avail)
            return buf;
        n--;
    }
    str_copy(buf, buf_sz, "...");
    return buf;
}

static void draw_widget(Screen *s, Gfx *g, Widget *w)
{
    if (!w->visible)
        return;

    int radius = 4;
    FontStyle ui_style = font_style_from_id(g, FONT_UI);
    int fh = font_char_h_style(g, &ui_style);

    switch (w->type) {
    case W_STATUSBAR:
        draw_statusbar(g, w);
        break;
    case W_SOFTKEYS:
        draw_softkeys(g, s, w);
        break;
    case W_LABEL: {
        FontId fid = (w->value > 0 || strstr(w->id, "clock_big") || strstr(w->id, "idle_clock")) ? FONT_MONO : FONT_UI;
        if (str_eq(w->component, "section_header") || str_starts(w->id, "sec_") || str_eq(w->id, "title"))
            fid = FONT_DISPLAY;
        FontStyle st = widget_font_style(g, w, fid);
        int lh = font_char_h_style(g, &st);
        int ty = w->y + (w->h - lh) / 2;
        if (w->h > lh * 2)
            ty = w->y + g->pad;
        if (str_eq(w->id, "boot_bar") || str_eq(w->id, "progress")) {
            int pct = clamp_i(w->value, 0, 100);
            int bh = clamp_i(w->h, fh + 8, w->h);
            int by = w->y + (w->h - bh) / 2;
            draw_round_rect(g, w->x, by, w->w, bh, radius, COL_PANEL, 1);
            draw_round_rect(g, w->x, by, w->w, bh, radius, COL_BORDER, 0);
            int fill = (w->w - 4) * pct / 100;
            if (fill > 0)
                draw_round_rect(g, w->x + 2, by + 2, fill, bh - 4, radius > 2 ? radius - 1 : 1, COL_ACCENT, 1);
        } else if (str_starts(w->id, "pill") || str_eq(w->component, "notification_pill") || w->value == 2) {
            FontStyle pill = widget_font_style(g, w, FONT_UI);
            int ph = clamp_i(font_char_h_style(g, &pill) + 8, 16, w->h > 0 ? w->h : font_char_h_style(g, &pill) + 8);
            int tw = font_text_w_style(g, &pill, w->text) + g->pad * 2 + 8;
            if (tw > w->w && w->w > 0)
                tw = w->w;
            int py = w->y + (w->h - ph) / 2;
            int pr = ph / 2;
            draw_round_rect(g, w->x, py, tw, ph, pr, w->bg && w->bg != COL_BG ? w->bg : COL_ACCENT, 1);
            font_draw_text_style_clip(g, &pill, w->x + g->pad + 2, py + (ph - font_char_h_style(g, &pill)) / 2,
                                      tw - g->pad * 2 - 4, w->text,
                                      w->fg ? w->fg : COL_ON_ACCENT,
                                      w->bg && w->bg != COL_BG ? w->bg : COL_ACCENT, 1);
        } else if (str_eq(w->component, "divider") || str_starts(w->id, "div")) {
            int mid = w->y + w->h / 2;
            draw_hline(g, w->x, mid, w->w, w->border_c ? w->border_c : COL_BORDER);
        } else if (str_eq(w->component, "section_header") || str_starts(w->id, "sec_")) {
            FontStyle hdr = widget_font_style(g, w, FONT_DISPLAY);
            font_draw_text_style_clip(g, &hdr, w->x, ty, w->w > 0 ? w->w : g->w, w->text,
                                      w->fg ? w->fg : COL_DIM, COL_BG, 0);
        } else {
            int br = w->radius > 0 ? clamp_i(w->radius, 2, 14) : 0;
            int pad_l = w->pad_l > 0 ? w->pad_l : 0;
            int pad_r = w->pad_r > 0 ? w->pad_r : 0;
            if (w->bg != 0 && w->bg != COL_BG && (w->radius > 0 || w->pad_t > 0 || strstr(w->id, "preview"))) {
                draw_round_rect(g, w->x, w->y, w->w, w->h, br > 0 ? br : radius, w->bg, 1);
                if (w->border > 0)
                    draw_round_rect(g, w->x, w->y, w->w, w->h, br > 0 ? br : radius, w->border_c ? w->border_c : COL_BORDER, 0);
                int clip_w = w->w - pad_l - pad_r;
                if (clip_w < 0)
                    clip_w = 0;
                char fit[MAX_TEXT + 4];
                const char *shown = label_text_fit(g, &st, w, clip_w, fit, sizeof(fit));
                int tx = label_text_x(g, &st, w, shown, w->x + pad_l, clip_w);
                font_draw_text_style_clip(g, &st, tx, ty, clip_w, shown, w->fg, w->bg, 0);
            } else {
                int avail = w->w > 0 ? w->w : g->w;
                char fit[MAX_TEXT + 4];
                const char *shown = label_text_fit(g, &st, w, avail, fit, sizeof(fit));
                int tx = label_text_x(g, &st, w, shown, w->x, avail);
                font_draw_text_style_clip(g, &st, tx, ty, avail, shown, w->fg, COL_BG, 0);
            }
        }
        break;
    }
    case W_BUTTON: {
        int primary = (w->bg == COL_ACCENT) || (w->fg == COL_ON_ACCENT && w->bg != COL_PANEL && w->bg != COL_BG);
        uint16_t bg = w->focused ? COL_ACCENT : (w->bg ? w->bg : COL_PANEL);
        uint16_t fg = w->focused ? COL_ON_ACCENT : (primary ? COL_ON_ACCENT : (w->fg ? w->fg : COL_TEXT));
        uint16_t border = w->focused ? COL_ACCENT : (primary ? COL_ACCENT : (w->border_c ? w->border_c : COL_BORDER));
        int br = w->radius > 0 ? clamp_i(w->radius, 2, 18) : radius;
        if (br * 2 > w->w)
            br = w->w / 2;
        if (br * 2 > w->h)
            br = w->h / 2;
        FontStyle btn_st = widget_font_style(g, w, FONT_UI);
        int btn_fh = font_char_h_style(g, &btn_st);
        if (w->icon[0] || str_eq(w->component, "launcher_tile") || str_eq(w->component, "tile")) {
            draw_elevated_rect(g, w->x, w->y, w->w, w->h, br, bg, border);
            if (g->composer)
                compose_set_clip(g->composer, w->x + 2, w->y + 2, w->w - 4, w->h - 4);
            int label_h = btn_fh + 6;
            int icon_band = w->h - label_h - 4;
            if (icon_band < 12)
                icon_band = w->h / 2;
            int icon_size = clamp_i(icon_band * 2 / 3, 14, 36);
            draw_app_icon(g, w->x + w->w / 2, w->y + 4 + icon_band / 2, icon_size, w->icon[0] ? w->icon : "app", fg);
            int label_y = w->y + w->h - label_h;
            int label_x = w->x + 4;
            int label_w = w->w - 8;
            if (label_w < 0)
                label_w = 0;
            font_draw_text_style_clip(g, &btn_st, label_x, label_y + (label_h - btn_fh) / 2, label_w, w->text, fg, bg, 1);
            if (g->composer)
                compose_set_clip(g->composer, 0, 0, g->w, g->h);
        } else if (str_eq(w->component, "message_row") || str_eq(w->component, "list_row") || str_eq(w->component, "row") ||
                   (w->value == 50 && w->parent >= 0 && s->widgets[w->parent].type == W_LIST)) {
            draw_round_rect(g, w->x, w->y, w->w, w->h, br > 0 ? br : 6, bg, 1);
            draw_round_rect(g, w->x, w->y, w->w, w->h, br > 0 ? br : 6, border, 0);
            int av = clamp_i(btn_fh + 10, 18, w->h - 8);
            int ax = w->x + 6;
            int ay = w->y + (w->h - av) / 2;
            draw_round_rect(g, ax, ay, av, av, av / 2, w->focused ? COL_ON_ACCENT : COL_ACCENT, 1);
            char initial[2] = {w->text[0] ? w->text[0] : '?', 0};
            int iw = font_text_w_style(g, &btn_st, initial);
            font_draw_text_style(g, &btn_st, ax + (av - iw) / 2, ay + (av - btn_fh) / 2, initial,
                                 w->focused ? COL_ACCENT : COL_ON_ACCENT,
                                 w->focused ? COL_ON_ACCENT : COL_ACCENT, 1);
            int tx = ax + av + 8;
            int tw = w->x + w->w - tx - 8;
            if (tw < 0)
                tw = 0;
            const char *sp = strchr(w->text, ' ');
            char title[64];
            char preview[96];
            title[0] = 0;
            preview[0] = 0;
            if (sp) {
                size_t n = (size_t)(sp - w->text);
                if (n >= sizeof(title))
                    n = sizeof(title) - 1;
                memcpy(title, w->text, n);
                title[n] = 0;
                str_copy(preview, sizeof(preview), sp + 1);
            } else {
                str_copy(title, sizeof(title), w->text);
            }
            FontStyle mono_st = font_style_from_id(g, FONT_MONO);
            int time_w = 0;
            const char *stamp = str_eq(w->component, "message_row") ? "now" : NULL;
            if (stamp) {
                time_w = font_text_w_style(g, &mono_st, stamp) + 4;
                if (time_w > tw / 3)
                    time_w = tw / 3;
            }
            int title_w = tw - time_w;
            if (title_w < 0)
                title_w = 0;
            font_draw_text_style_clip(g, &btn_st, tx, w->y + 4, title_w, title, fg, bg, 1);
            if (stamp && time_w > 0)
                font_draw_text_style_clip(g, &mono_st, tx + title_w, w->y + 4, time_w, stamp, COL_DIM, bg, 1);
            if (preview[0])
                font_draw_text_style_clip(g, &btn_st, tx, w->y + 4 + btn_fh + 2, tw, preview, COL_DIM, bg, 1);
            if (w->value > 0 && str_eq(w->component, "message_row")) {
                int bx = w->x + w->w - 14;
                int by = w->y + 8;
                draw_round_rect(g, bx, by, 8, 8, 4, COL_ACCENT, 1);
            }
        } else {
            if (primary)
                draw_round_rect(g, w->x, w->y, w->w, w->h, br, bg, 1);
            else
                draw_elevated_rect(g, w->x, w->y, w->w, w->h, br, bg, border);
            if (primary)
                draw_round_rect(g, w->x, w->y, w->w, w->h, br, border, 0);
            int pad = g->pad > 0 ? g->pad : 4;
            int tw_avail = w->w - pad * 2;
            if (tw_avail < 0)
                tw_avail = 0;
            int text_w = font_text_w_style(g, &btn_st, w->text);
            int tx = w->x + pad;
            if (text_w < tw_avail)
                tx = w->x + (w->w - text_w) / 2;
            int ty = w->y + (w->h - btn_fh) / 2;
            font_draw_text_style_clip(g, &btn_st, tx, ty, tw_avail, w->text, fg, bg, 1);
        }
        break;
    }
    case W_INPUT: {
        uint16_t bg = w->focused ? COL_PANEL : (w->bg ? w->bg : COL_PANEL);
        uint16_t fg = w->fg ? w->fg : COL_TEXT;
        FontStyle in_st = widget_font_style(g, w, FONT_MONO);
        int ih = font_char_h_style(g, &in_st);
        int br = w->radius > 0 ? clamp_i(w->radius, 2, 12) : radius;
        draw_round_rect(g, w->x, w->y, w->w, w->h, br, bg, 1);
        draw_round_rect(g, w->x, w->y, w->w, w->h, br, w->focused ? COL_ACCENT : COL_BORDER, 0);
        int pad = g->pad > 0 ? g->pad : 6;
        font_draw_text_style_clip(g, &in_st, w->x + pad, w->y + (w->h - ih) / 2, w->w - pad * 2, w->text, fg, bg, 1);
        if (w->focused) {
            int tw = font_text_w_style(g, &in_st, w->text);
            if (tw > w->w - pad * 2)
                tw = w->w - pad * 2;
            draw_vline(g, w->x + pad + tw + 1, w->y + 4, w->h - 8, COL_ACCENT);
        }
        break;
    }
    case W_LIST: {
        int radius_l = w->radius > 0 ? clamp_i(w->radius, 2, 16) : radius;
        draw_round_rect(g, w->x, w->y, w->w, w->h, radius_l, w->bg ? w->bg : COL_PANEL, 1);
        draw_round_rect(g, w->x, w->y, w->w, w->h, radius_l, w->border_c ? w->border_c : COL_BORDER, 0);
        if (g->composer)
            compose_set_clip(g->composer, w->x + 1, w->y + 1, w->w - 2, w->h - 2);
        for (int i = 0; i < w->child_count; i++) {
            Widget *item = &s->widgets[w->children[i]];
            if (!item->visible)
                continue;
            if (item->y < w->y || item->y + item->h > w->y + w->h)
                continue;
            if (item->x + item->w <= w->x || item->x >= w->x + w->w)
                continue;
            draw_widget(s, g, item);
        }
        if (g->composer)
            compose_set_clip(g->composer, 0, 0, g->w, g->h);
        break;
    }
    case W_GRID:
    case W_VBOX:
    case W_HBOX:
    case W_FLEX:
    case W_SCREEN:
        draw_container(s, g, w);
        break;
    default:
        break;
    }
}

static void draw_container_children(Screen *s, Gfx *g, Widget *w)
{
    for (int i = 0; i < w->child_count; i++)
        draw_widget(s, g, &s->widgets[w->children[i]]);
}

static void draw_cursor(Gfx *g, const InputState *in)
{
    if (!in || !in->cursor_visible)
        return;
    int x = in->cursor_x;
    int y = in->cursor_y;
    draw_line(g, x + 1, y + 1, x + 9, y + 9, COL_ON_ACCENT);
    draw_line(g, x + 1, y + 1, x + 1, y + 13, COL_ON_ACCENT);
    draw_line(g, x + 1, y + 13, x + 5, y + 10, COL_ON_ACCENT);
    draw_line(g, x + 5, y + 10, x + 9, y + 9, COL_ON_ACCENT);
    draw_line(g, x, y, x + 8, y + 8, COL_ACCENT);
    draw_line(g, x, y, x, y + 12, COL_ACCENT);
    draw_line(g, x, y + 12, x + 4, y + 9, COL_ACCENT);
    draw_line(g, x + 4, y + 9, x + 8, y + 8, COL_ACCENT);
    draw_pixel(g, x + 1, y + 1, COL_TEXT);
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
}

void screen_set_mode_badge(Screen *s, const char *mode)
{
    Widget *b = screen_find(s, "mode_badge");
    if (b && mode)
        str_copy(b->text, sizeof(b->text), mode);
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
}

static Widget *primary_input(Screen *s)
{
    if (!s)
        return NULL;
    if (s->focus >= 0 && s->focus < s->count && s->widgets[s->focus].type == W_INPUT)
        return &s->widgets[s->focus];
    for (int i = 0; i < s->count; i++) {
        if (s->widgets[i].type == W_INPUT && s->widgets[i].visible)
            return &s->widgets[i];
    }
    return NULL;
}

static int append_char(Screen *s, char ch)
{
    Widget *in = primary_input(s);
    if (!in)
        return 0;
    size_t n = strlen(in->text);
    if ((int)n < in->max_len && n + 1 < sizeof(in->text)) {
        in->text[n] = ch;
        in->text[n + 1] = 0;
    }
    return 2;
}

static void nav_to(char *out_nav, size_t out_sz, const char *target)
{
    if (out_nav)
        str_copy(out_nav, out_sz, target);
}

static int action_is_path(const char *action)
{
    return strchr(action, '/') != NULL || strstr(action, ".xml") != NULL;
}

static int thin_call(Screen *s, char *out_nav, size_t out_sz)
{
    Widget *in = primary_input(s);
    const char *n = (in && in->text[0]) ? in->text : "555-0101";
    const Contact *c = contacts_by_number(n);
    screen_carry_text("call_number", n);
    screen_carry_text("call_name", c ? c->name : "Calling...");
    nav_to(out_nav, out_sz, "route:replace:res/layout/call_active.xml");
    return 1;
}

int screen_handle_action(Screen *s, const char *action, char *out_nav, size_t out_sz)
{
    if (!action || !action[0] || str_eq(action, "none"))
        return 0;
    if (str_starts(action, "nav:")) {
        nav_to(out_nav, out_sz, action + 4);
        return 1;
    }
    if (str_eq(action, "back")) {
        if (strstr(s->path, "idle.xml")) {
            if (out_nav)
                out_nav[0] = 0;
            return 0;
        }
        if (strstr(s->path, "home.xml")) {
            nav_to(out_nav, out_sz, "assets/screens/idle.xml");
            return 1;
        }
        if (strstr(s->path, "message_thread") || strstr(s->path, "compose")) {
            nav_to(out_nav, out_sz, "assets/screens/messages.xml");
            return 1;
        }
        if (strstr(s->path, "contact_detail")) {
            nav_to(out_nav, out_sz, "assets/screens/contacts.xml");
            return 1;
        }
        if (strstr(s->path, "call_active")) {
            nav_to(out_nav, out_sz, "assets/screens/dialer.xml");
            return 1;
        }
        nav_to(out_nav, out_sz, "assets/screens/home.xml");
        return 1;
    }
    if (str_eq(action, "quit") || str_eq(action, "exit") || str_eq(action, "lock")) {
        nav_to(out_nav, out_sz, "assets/screens/idle.xml");
        return 1;
    }
    if (str_eq(action, "unlock") || str_eq(action, "menu")) {
        nav_to(out_nav, out_sz, "assets/screens/home.xml");
        return 1;
    }
    if (str_eq(action, "power") || str_eq(action, "power_menu") || str_eq(action, "options")) {
        nav_to(out_nav, out_sz, "assets/screens/power.xml");
        return 1;
    }
    if (str_eq(action, "power_cancel") || str_eq(action, "cancel")) {
        nav_to(out_nav, out_sz, "assets/screens/home.xml");
        return 1;
    }
    if (str_eq(action, "power_off"))
        return 30;
    if (str_eq(action, "power_restart"))
        return 31;
    if (str_eq(action, "clear")) {
        Widget *in = primary_input(s);
        if (in)
            in->text[0] = 0;
        return 2;
    }
    if (str_starts(action, "digit:") && action[6]) {
        append_char(s, action[6]);
        return 2;
    }
    if (str_starts(action, "key:") && action[4]) {
        append_char(s, action[4]);
        return 2;
    }
    if (str_eq(action, "call"))
        return thin_call(s, out_nav, out_sz);
    if (str_eq(action, "end_call")) {
        nav_to(out_nav, out_sz, "route:pop");
        return 1;
    }
    if (str_eq(action, "select") || str_eq(action, "ok")) {
        if (s && s->focus >= 0) {
            Widget *w = &s->widgets[s->focus];
            if (w->action[0] && !str_eq(w->action, "ok") && !str_eq(w->action, "select"))
                return screen_handle_action(s, w->action, out_nav, out_sz);
        }
        return 2;
    }
    if (!action_is_path(action) && !str_starts(action, "route:") && !str_starts(action, "app:"))
        return 0;
    nav_to(out_nav, out_sz, action);
    return 1;
}
