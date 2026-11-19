#include "screen.h"
#include "xml.h"
#include "xml_ui.h"
#include "draw.h"
#include "font.h"
#include "fb.h"
#include "util.h"
#include "procman.h"
#include "compose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static XmlUiResources g_ui_res;

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
    if (str_eq(action, "power") || str_eq(action, "power_menu") || str_eq(action, "options")) {
        if (out_nav)
            str_copy(out_nav, out_sz, "assets/screens/power.xml");
        return 1;
    }
    if (str_eq(action, "power_cancel") || str_eq(action, "cancel")) {
        if (out_nav)
            str_copy(out_nav, out_sz, "assets/screens/home.xml");
        return 1;
    }
    if (str_eq(action, "power_off"))
        return 30;
    if (str_eq(action, "power_restart"))
        return 31;
    if (str_eq(action, "clear")) {
        if (s && s->focus >= 0) {
            Widget *w = &s->widgets[s->focus];
            if (w->type == W_INPUT)
                w->text[0] = 0;
        }
        Widget *num = screen_find(s, "number");
        if (num)
            num->text[0] = 0;
        Widget *note = screen_find(s, "note");
        if (note && (!num || s->focus == (int)(note - s->widgets)))
            note->text[0] = 0;
        Widget *msg = screen_find(s, "msg");
        if (msg && s->focus >= 0 && &s->widgets[s->focus] == msg)
            msg->text[0] = 0;
        Widget *match = screen_find(s, "contact_match");
        if (match)
            str_copy(match->text, sizeof(match->text), "Enter a number");
        return 2;
    }
    if (str_starts(action, "digit:")) {
        append_digit(s, action[6]);
        return 2;
    }
    if (str_starts(action, "key:")) {
        append_digit(s, action[4]);
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
            str_copy(out_nav, out_sz, "route:replace:res/layout/call_active.xml");
        return 1;
    }
    if (str_eq(action, "end_call")) {
        g_app.call_start = 0;
        if (out_nav)
            str_copy(out_nav, out_sz, "route:pop");
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
            body = screen_find(s, "msg_input");
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
