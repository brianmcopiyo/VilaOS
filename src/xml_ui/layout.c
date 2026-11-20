#include "xml_ui.h"
#include "font.h"
#include "fb.h"
#include "util.h"

#include <string.h>

int xml_ui_resolve_dim(int src, unsigned unit, int parent, int screen, int ref)
{
    (void)screen;
    (void)ref;
    if (unit == UNIT_PCT)
        return (parent * src) / 100;
    if (unit == UNIT_AUTO || unit == UNIT_FILL)
        return 0;
    return src;
}

static int scale_space(Gfx *g, int v)
{
    (void)g;
    return v;
}

static int grid_auto_cols(Gfx *g, Widget *grid)
{
    if (grid->grid_cols > 0)
        return grid->grid_cols;
    int avail = grid->w > 0 ? grid->w : g->w;
    float ar = g->aspect_ratio > 0.01f ? g->aspect_ratio : ((g->h > 0) ? ((float)g->w / (float)g->h) : 1.0f);
    int min_cell = 88;
    if (ar >= 1.2f)
        min_cell = 80;
    if (ar >= 1.6f)
        min_cell = 72;
    if (avail < 280)
        min_cell = 72;
    int cols = min_cell > 0 ? (avail / min_cell) : 4;
    if (cols < 3)
        cols = 3;
    if (cols > 5)
        cols = 5;
    if (ar < 0.85f)
        cols = 4;
    if (ar > 1.3f && cols < 5 && avail >= 400)
        cols = 5;
    return cols;
}

static int clamp_size(int v, int mn, int mx)
{
    if (mn > 0 && v < mn)
        v = mn;
    if (mx > 0 && v > mx)
        v = mx;
    return v;
}

static int pad_px(Gfx *g, int16_t v)
{
    return v < 0 ? g->pad : scale_space(g, v);
}

static int pad_raw(int16_t v)
{
    return v > 0 ? v : 0;
}

static int is_chrome(const Widget *w)
{
    return w && (w->type == W_STATUSBAR || w->type == W_SOFTKEYS);
}

static int intrinsic_h(Gfx *g, Widget *w)
{
    int fh = font_char_h_scaled(g, FONT_8X12);
    switch (w->type) {
    case W_LABEL:
        return fh + scale_space(g, 6) + scale_space(g, pad_raw(w->pad_t) + pad_raw(w->pad_b));
    case W_BUTTON:
    case W_INPUT:
        return g->item_h + scale_space(g, pad_raw(w->pad_t) + pad_raw(w->pad_b));
    case W_LIST:
        return g->item_h * 3;
    case W_GRID:
        return g->item_h * 2;
    default:
        return g->item_h;
    }
}

static int intrinsic_w(Gfx *g, Widget *w)
{
    int tw = font_text_w_scaled(g, FONT_8X12, w->text);
    switch (w->type) {
    case W_LABEL:
        return tw + scale_space(g, pad_raw(w->pad_l) + pad_raw(w->pad_r)) + 4;
    case W_BUTTON:
        if (w->icon[0])
            return g->item_h * 2;
        return tw + g->pad * 2 + scale_space(g, pad_raw(w->pad_l) + pad_raw(w->pad_r));
    default:
        return g->item_h * 2;
    }
}

static void measure_widget(Screen *s, Gfx *g, Widget *w);

static void measure_flex(Screen *s, Gfx *g, Widget *box)
{
    int vertical = box->flex_dir != FLEX_ROW;
    int gap = scale_space(g, box->gap > 0 ? box->gap : (box->row_gap > 0 || box->col_gap > 0 ? 0 : g->pad / 2 + 2));
    int row_gap = scale_space(g, box->row_gap > 0 ? box->row_gap : (vertical ? (box->gap > 0 ? box->gap : g->pad / 2 + 2) : 0));
    int col_gap = scale_space(g, box->col_gap > 0 ? box->col_gap : (!vertical ? (box->gap > 0 ? box->gap : g->pad / 2 + 2) : 0));
    if (vertical)
        gap = row_gap > 0 ? row_gap : gap;
    else
        gap = col_gap > 0 ? col_gap : gap;

    int pad_t = pad_px(g, box->pad_t);
    int pad_r = pad_px(g, box->pad_r);
    int pad_b = pad_px(g, box->pad_b);
    int pad_l = pad_px(g, box->pad_l);

    int main = 0;
    int cross = 0;
    int count = 0;
    for (int i = 0; i < box->child_count; i++) {
        Widget *c = &s->widgets[box->children[i]];
        if (is_chrome(c))
            continue;
        measure_widget(s, g, c);
        int mw = c->meas_w + scale_space(g, c->margin_l + c->margin_r);
        int mh = c->meas_h + scale_space(g, c->margin_t + c->margin_b);
        if (vertical) {
            main += mh;
            if (mw > cross)
                cross = mw;
        } else {
            main += mw;
            if (mh > cross)
                cross = mh;
        }
        count++;
    }
    if (count > 1)
        main += gap * (count - 1);
    main += vertical ? (pad_t + pad_b) : (pad_l + pad_r);
    cross += vertical ? (pad_l + pad_r) : (pad_t + pad_b);
    box->meas_w = cross > 0 ? cross : g->item_h;
    box->meas_h = main > 0 ? main : g->item_h;
}

static void measure_grid(Screen *s, Gfx *g, Widget *grid)
{
    int cols = grid_auto_cols(g, grid);
    int n = 0;
    for (int i = 0; i < grid->child_count; i++) {
        Widget *c = &s->widgets[grid->children[i]];
        if (!c->visible)
            continue;
        if (c->type == W_BUTTON || c->type == W_LABEL) {
            measure_widget(s, g, c);
            n++;
        }
    }
    int rows = n > 0 ? (n + cols - 1) / cols : 1;
    int gap = scale_space(g, grid->gap > 0 ? grid->gap : g->pad);
    int pad = pad_px(g, grid->pad_t);
    int cell = 76;
    if (g->aspect_ratio >= 1.2f)
        cell = 72;
    grid->meas_w = cols * cell + (cols - 1) * gap + pad * 2;
    grid->meas_h = rows * (cell + 22) + (rows - 1) * gap + pad * 2;
}

static void measure_list(Screen *s, Gfx *g, Widget *list)
{
    int gap = scale_space(g, list->gap > 0 ? list->gap : 2);
    int total = 0;
    int n = 0;
    for (int i = 0; i < list->child_count; i++) {
        Widget *c = &s->widgets[list->children[i]];
        measure_widget(s, g, c);
        int ch = c->meas_h > 0 ? c->meas_h : g->item_h;
        total += ch;
        n++;
    }
    if (n > 1)
        total += (n - 1) * gap;
    int pad = scale_space(g, pad_raw(list->pad_t) + pad_raw(list->pad_b)) + 4;
    list->meas_w = g->item_h * 4;
    list->meas_h = total + pad;
    if (list->meas_h < g->item_h * 2)
        list->meas_h = g->item_h * 2;
}

static void measure_widget(Screen *s, Gfx *g, Widget *w)
{
    if (!w)
        return;
    int rw = xml_ui_resolve_dim(w->sw, w->wu, 0, g->w, g->w);
    int rh = xml_ui_resolve_dim(w->sh, w->hu, 0, g->h, g->h);

    if (w->type == W_VBOX || w->type == W_HBOX || w->type == W_FLEX) {
        if (w->type == W_VBOX)
            w->flex_dir = FLEX_COL;
        if (w->type == W_HBOX)
            w->flex_dir = FLEX_ROW;
        measure_flex(s, g, w);
    } else if (w->type == W_GRID) {
        measure_grid(s, g, w);
    } else if (w->type == W_LIST) {
        measure_list(s, g, w);
    } else {
        w->meas_w = rw > 0 ? rw : intrinsic_w(g, w);
        w->meas_h = rh > 0 ? rh : intrinsic_h(g, w);
    }

    if (rw > 0)
        w->meas_w = rw;
    if (rh > 0)
        w->meas_h = rh;

    int min_w = scale_space(g, w->min_w);
    int min_h = scale_space(g, w->min_h);
    int max_w = scale_space(g, w->max_w);
    int max_h = scale_space(g, w->max_h);
    w->meas_w = (int16_t)clamp_size(w->meas_w, min_w, max_w);
    w->meas_h = (int16_t)clamp_size(w->meas_h, min_h, max_h);
}

static void arrange_list(Screen *s, Gfx *g, Widget *list)
{
    int pad = 2 + scale_space(g, pad_raw(list->pad_l));
    int pad_t = 2 + scale_space(g, pad_raw(list->pad_t));
    int gap = scale_space(g, list->gap > 0 ? list->gap : 2);
    int iy = list->y + pad_t - list->scroll;
    int inner_w = list->w - pad - 2 - scale_space(g, pad_raw(list->pad_r));
    if (inner_w < 8)
        inner_w = 8;
    for (int i = 0; i < list->child_count; i++) {
        Widget *item = &s->widgets[list->children[i]];
        int item_h = item->meas_h > 0 ? item->meas_h : g->item_h;
        int rh = xml_ui_resolve_dim(item->sh, item->hu, 0, g->h, g->h);
        if (rh > 0)
            item_h = rh;
        item->x = list->x + pad;
        item->y = iy;
        item->w = inner_w;
        item->h = item_h;
        item->focusable = 1;
        item->visible = (item->y + item->h > list->y && item->y < list->y + list->h);
        iy += item_h + gap;
    }
}

static void arrange_widget(Screen *s, Gfx *g, Widget *w);

static void arrange_grid(Screen *s, Gfx *g, Widget *grid)
{
    int cols = grid_auto_cols(g, grid);
    if (cols < 1)
        cols = 1;
    int pad_t = pad_px(g, grid->pad_t);
    int pad_r = pad_px(g, grid->pad_r);
    int pad_b = pad_px(g, grid->pad_b);
    int pad_l = pad_px(g, grid->pad_l);
    int gap = scale_space(g, grid->gap > 0 ? grid->gap : g->pad);
    int row_gap = scale_space(g, grid->row_gap > 0 ? grid->row_gap : gap);
    int col_gap = scale_space(g, grid->col_gap > 0 ? grid->col_gap : gap);

    int place_r[MAX_CHILDREN];
    int place_c[MAX_CHILDREN];
    int place_span[MAX_CHILDREN];
    int place_idx[MAX_CHILDREN];
    int n = 0;
    int cursor = 0;
    int max_row = 0;
    for (int i = 0; i < grid->child_count && n < MAX_CHILDREN; i++) {
        Widget *c = &s->widgets[grid->children[i]];
        if (is_chrome(c))
            continue;
        if (!c->visible) {
            c->focusable = 0;
            c->w = 0;
            c->h = 0;
            continue;
        }
        int span = c->col_span > 0 ? c->col_span : 1;
        if (span > cols)
            span = cols;
        int col = cursor % cols;
        if (col + span > cols) {
            cursor += cols - col;
            col = 0;
        }
        int r = cursor / cols;
        place_idx[n] = grid->children[i];
        place_r[n] = r;
        place_c[n] = col;
        place_span[n] = span;
        if (r > max_row)
            max_row = r;
        cursor += span;
        n++;
    }
    if (n <= 0)
        return;
    int rows = max_row + 1;
    if (grid->grid_rows > rows)
        rows = grid->grid_rows;

    int inner_w = grid->w - pad_l - pad_r - col_gap * (cols > 1 ? cols - 1 : 0);
    int inner_h = grid->h - pad_t - pad_b - row_gap * (rows > 1 ? rows - 1 : 0);
    if (inner_w < cols)
        inner_w = cols;
    if (inner_h < rows)
        inner_h = rows;

    int only_tiles = 1;
    for (int i = 0; i < n; i++) {
        Widget *c = &s->widgets[place_idx[i]];
        if (c->type != W_BUTTON && c->type != W_LABEL) {
            only_tiles = 0;
            break;
        }
        if (c->type == W_BUTTON && !c->icon[0] && !str_eq(c->component, "tile") &&
            !str_eq(c->component, "launcher_tile")) {
            only_tiles = 0;
            break;
        }
    }

    int col_w[16];
    int row_h[16];
    for (int c = 0; c < cols && c < 16; c++)
        col_w[c] = 0;
    for (int r = 0; r < rows && r < 16; r++)
        row_h[r] = 0;

    if (!only_tiles && cols == 2 && cols <= 16) {
        int side = grid->max_w > 0 ? grid->max_w : (inner_w * 2 / 5);
        if (side > inner_w / 2)
            side = inner_w * 2 / 5;
        if (side < 1)
            side = inner_w / 3;
        if (side < 1)
            side = 1;
        col_w[0] = inner_w - side;
        col_w[1] = side;
    } else {
        int cw = inner_w / cols;
        if (cw < 1)
            cw = 1;
        for (int c = 0; c < cols && c < 16; c++)
            col_w[c] = cw;
    }

    if (!only_tiles && rows >= 3 && rows <= 16) {
        for (int i = 0; i < n; i++) {
            Widget *c = &s->widgets[place_idx[i]];
            measure_widget(s, g, c);
            int prefer = c->meas_h > 0 ? c->meas_h : intrinsic_h(g, c);
            int pad_extra = pad_px(g, c->pad_t) + pad_px(g, c->pad_b) + 8;
            prefer += pad_extra;
            int r = place_r[i];
            if (r == 0 || r == rows - 1) {
                if (prefer > row_h[r])
                    row_h[r] = prefer;
            }
        }
        if (row_h[0] < 70)
            row_h[0] = 70;
        if (row_h[0] > 88)
            row_h[0] = 88;
        if (row_h[0] > inner_h / 4)
            row_h[0] = inner_h / 4;
        if (row_h[rows - 1] < 40)
            row_h[rows - 1] = 40;
        if (row_h[rows - 1] > 52)
            row_h[rows - 1] = 52;
        if (row_h[rows - 1] > inner_h / 6)
            row_h[rows - 1] = inner_h / 6;
        int fixed = 0;
        for (int r = 0; r < rows; r++) {
            if (r == 0 || r == rows - 1)
                fixed += row_h[r];
        }
        int star = rows - 2;
        int rem = inner_h - fixed;
        if (rem < star)
            rem = star;
        int mid = star > 0 ? rem / star : rem;
        for (int r = 1; r < rows - 1; r++)
            row_h[r] = mid;
    } else {
        int rh = inner_h / rows;
        if (rh < 1)
            rh = 1;
        for (int r = 0; r < rows && r < 16; r++)
            row_h[r] = rh;
    }

    float ar = g->aspect_ratio > 0.01f ? g->aspect_ratio : 1.0f;
    if (only_tiles) {
        int cell_w = col_w[0];
        int cell_h = row_h[0];
        if (ar > 1.3f && cell_h > cell_w + 12)
            cell_h = cell_w + 8;
        if (ar < 0.8f && cell_w > cell_h + 16)
            cell_w = cell_h + 8;
        int max_cell = cell_w < cell_h ? cell_w : cell_h;
        if (max_cell > 96) {
            cell_w = 96;
            cell_h = 96;
        }
        for (int c = 0; c < cols && c < 16; c++)
            col_w[c] = cell_w;
        for (int r = 0; r < rows && r < 16; r++)
            row_h[r] = cell_h;
    }

    int col_x[16];
    int row_y[16];
    int x0 = grid->x + pad_l;
    for (int c = 0; c < cols && c < 16; c++) {
        col_x[c] = x0;
        x0 += col_w[c] + (c + 1 < cols ? col_gap : 0);
    }
    int y0 = grid->y + pad_t;
    for (int r = 0; r < rows && r < 16; r++) {
        row_y[r] = y0;
        y0 += row_h[r] + (r + 1 < rows ? row_gap : 0);
    }

    for (int i = 0; i < n; i++) {
        Widget *c = &s->widgets[place_idx[i]];
        int r = place_r[i];
        int col = place_c[i];
        int span = place_span[i];
        int wsum = 0;
        for (int k = 0; k < span && col + k < cols && col + k < 16; k++) {
            wsum += col_w[col + k];
            if (k + 1 < span)
                wsum += col_gap;
        }
        c->w = wsum;
        c->h = (r < 16) ? row_h[r] : 1;
        c->x = (col < 16) ? col_x[col] : grid->x + pad_l;
        c->y = (r < 16) ? row_y[r] : grid->y + pad_t;
        if (c->x + c->w > grid->x + grid->w - pad_r)
            c->w = grid->x + grid->w - pad_r - c->x;
        if (c->y + c->h > grid->y + grid->h - pad_b)
            c->h = grid->y + grid->h - pad_b - c->y;
        if (c->w < 0)
            c->w = 0;
        if (c->h < 0)
            c->h = 0;
        if (c->type == W_BUTTON || c->type == W_INPUT)
            c->focusable = 1;
        arrange_widget(s, g, c);
    }
}

static void arrange_flex(Screen *s, Gfx *g, Widget *box)
{
    int vertical = box->flex_dir != FLEX_ROW;
    int gap = scale_space(g, box->gap > 0 ? box->gap : g->pad / 2 + 2);
    int row_gap = scale_space(g, box->row_gap > 0 ? box->row_gap : gap);
    int col_gap = scale_space(g, box->col_gap > 0 ? box->col_gap : gap);
    gap = vertical ? row_gap : col_gap;

    int pad_t = pad_px(g, box->pad_t);
    int pad_r = pad_px(g, box->pad_r);
    int pad_b = pad_px(g, box->pad_b);
    int pad_l = pad_px(g, box->pad_l);

    int inner_w = box->w - pad_l - pad_r;
    int inner_h = box->h - pad_t - pad_b;
    if (inner_w < 8)
        inner_w = 8;
    if (inner_h < 8)
        inner_h = 8;

    typedef struct {
        int idx;
        int base;
        int margin_main0;
        int margin_main1;
        int margin_cross0;
        int margin_cross1;
        int grow;
        int fixed;
    } FlexItem;

    FlexItem items[MAX_CHILDREN];
    int n = 0;
    int fixed_sum = 0;
    int grow_sum = 0;
    int gaps = 0;

    for (int i = 0; i < box->child_count && n < MAX_CHILDREN; i++) {
        Widget *c = &s->widgets[box->children[i]];
        if (is_chrome(c))
            continue;
        FlexItem *it = &items[n++];
        it->idx = box->children[i];
        it->margin_main0 = scale_space(g, vertical ? c->margin_t : c->margin_l);
        it->margin_main1 = scale_space(g, vertical ? c->margin_b : c->margin_r);
        it->margin_cross0 = scale_space(g, vertical ? c->margin_l : c->margin_t);
        it->margin_cross1 = scale_space(g, vertical ? c->margin_r : c->margin_b);
        unsigned main_u = vertical ? c->hu : c->wu;
        {
            int auto_grow = (c->type == W_LIST || c->type == W_GRID || c->type == W_VBOX || c->type == W_HBOX ||
                             c->type == W_FLEX);
            if (c->grow > 0)
                it->grow = c->grow;
            else if (main_u == UNIT_FILL)
                it->grow = 1;
            else if (main_u == UNIT_AUTO && auto_grow)
                it->grow = 1;
            else
                it->grow = 0;
        }
        int want = vertical ? xml_ui_resolve_dim(c->sh, c->hu, inner_h, g->h, g->h)
                            : xml_ui_resolve_dim(c->sw, c->wu, inner_w, g->w, g->w);
        int pct_fill = (main_u == UNIT_PCT && ((vertical && c->sh >= 100) || (!vertical && c->sw >= 100)));
        if (pct_fill || main_u == UNIT_FILL) {
            if (it->grow <= 0)
                it->grow = 1;
            it->base = 0;
            it->fixed = 0;
        } else if (want > 0) {
            it->base = want;
            it->fixed = 1;
            it->grow = 0;
        } else if (it->grow > 0) {
            it->base = vertical ? (c->type == W_LABEL ? intrinsic_h(g, c) : 0)
                                : (c->type == W_LABEL ? intrinsic_w(g, c) : 0);
            if (c->type == W_LABEL || c->type == W_BUTTON || c->type == W_INPUT) {
                it->base = vertical ? intrinsic_h(g, c) : intrinsic_w(g, c);
                if (c->type == W_LABEL) {
                    it->grow = 0;
                    it->fixed = 1;
                }
            }
            it->fixed = 0;
        } else {
            it->base = vertical ? (c->meas_h > 0 ? c->meas_h : intrinsic_h(g, c))
                                : (c->meas_w > 0 ? c->meas_w : intrinsic_w(g, c));
            it->fixed = 1;
        }
        if (it->grow == 0)
            fixed_sum += it->base + it->margin_main0 + it->margin_main1;
        else
            fixed_sum += it->margin_main0 + it->margin_main1 + it->base;
        grow_sum += it->grow;
    }
    if (n > 1)
        gaps = gap * (n - 1);
    int main_space = (vertical ? inner_h : inner_w) - fixed_sum - gaps;
    if (main_space < 0)
        main_space = 0;

    int content_x0 = box->x + pad_l;
    int content_y0 = box->y + pad_t - (vertical ? box->scroll : 0);
    int content_x1 = box->x + box->w - pad_r;
    int content_y1 = box->y + box->h - pad_b;
    int lead = 0;
    int extra_gap = 0;
    int extra_rem = 0;
    if (grow_sum == 0 && n > 0) {
        if (box->justify == JUSTIFY_CENTER) {
            lead = main_space / 2;
        } else if (box->justify == JUSTIFY_END) {
            lead = main_space;
        } else if (box->justify == JUSTIFY_BETWEEN && n > 1) {
            extra_gap = main_space / (n - 1);
            extra_rem = main_space % (n - 1);
        }
    }
    int edge = (vertical ? content_y0 : content_x0) + lead;

    for (int i = 0; i < n; i++) {
        int gap_i = gap + extra_gap + (i < extra_rem ? 1 : 0);
        FlexItem *it = &items[i];
        Widget *c = &s->widgets[it->idx];
        int main_sz = it->base;
        if (it->grow > 0 && grow_sum > 0)
            main_sz += (main_space * it->grow) / grow_sum;
        main_sz = clamp_size(main_sz, scale_space(g, vertical ? c->min_h : c->min_w),
                             scale_space(g, vertical ? c->max_h : c->max_w));

        int avail = (vertical ? content_y1 : content_x1) - edge - it->margin_main0 - it->margin_main1;
        if (avail < 0)
            avail = 0;
        if (main_sz > avail)
            main_sz = avail;

        int cross_sz;
        if (vertical) {
            int rw = xml_ui_resolve_dim(c->sw, c->wu, inner_w, g->w, g->w);
            cross_sz = rw > 0 ? rw : (inner_w - it->margin_cross0 - it->margin_cross1);
            if (box->align == ALIGN_STRETCH && (c->wu == UNIT_AUTO || c->wu == UNIT_FILL || rw <= 0))
                cross_sz = inner_w - it->margin_cross0 - it->margin_cross1;
        } else {
            int rh = xml_ui_resolve_dim(c->sh, c->hu, inner_h, g->h, g->h);
            cross_sz = rh > 0 ? rh : (inner_h - it->margin_cross0 - it->margin_cross1);
            if (box->align == ALIGN_STRETCH && (c->hu == UNIT_AUTO || c->hu == UNIT_FILL || rh <= 0))
                cross_sz = inner_h - it->margin_cross0 - it->margin_cross1;
        }
        cross_sz = clamp_size(cross_sz, scale_space(g, vertical ? c->min_w : c->min_h),
                              scale_space(g, vertical ? c->max_w : c->max_h));
        if (cross_sz < 1)
            cross_sz = 1;

        if (vertical) {
            c->y = edge + it->margin_main0;
            c->h = main_sz;
            c->w = cross_sz;
            if (box->align == ALIGN_CENTER)
                c->x = content_x0 + it->margin_cross0 + (inner_w - it->margin_cross0 - it->margin_cross1 - cross_sz) / 2;
            else if (box->align == ALIGN_END)
                c->x = content_x1 - it->margin_cross1 - cross_sz;
            else
                c->x = content_x0 + it->margin_cross0;
            edge = c->y + c->h + it->margin_main1 + (i + 1 < n ? gap_i : 0);
        } else {
            c->x = edge + it->margin_main0;
            c->w = main_sz;
            c->h = cross_sz;
            if (box->align == ALIGN_CENTER)
                c->y = content_y0 + it->margin_cross0 + (inner_h - it->margin_cross0 - it->margin_cross1 - cross_sz) / 2;
            else if (box->align == ALIGN_END)
                c->y = content_y1 - it->margin_cross1 - cross_sz;
            else
                c->y = content_y0 + it->margin_cross0;
            edge = c->x + c->w + it->margin_main1 + (i + 1 < n ? gap_i : 0);
        }
        if (c->w < 0)
            c->w = 0;
        if (c->h < 0)
            c->h = 0;
        c->visible = (c->w > 0 && c->h > 0);
        arrange_widget(s, g, c);
    }
}

static void arrange_widget(Screen *s, Gfx *g, Widget *w)
{
    if (!w)
        return;
    if (w->type == W_SCREEN) {
        w->flex_dir = FLEX_COL;
        arrange_flex(s, g, w);
    } else if (w->type == W_VBOX || w->type == W_FLEX) {
        if (w->type == W_VBOX)
            w->flex_dir = FLEX_COL;
        arrange_flex(s, g, w);
    } else if (w->type == W_HBOX) {
        w->flex_dir = FLEX_ROW;
        arrange_flex(s, g, w);
    } else if (w->type == W_GRID) {
        arrange_grid(s, g, w);
    } else if (w->type == W_LIST) {
        arrange_list(s, g, w);
    }
}

void xml_ui_clip_children(Screen *s, Widget *container)
{
    if (!s || !container)
        return;
    for (int i = 0; i < container->child_count; i++) {
        Widget *c = &s->widgets[container->children[i]];
        if (c->x < container->x) {
            c->w -= (container->x - c->x);
            c->x = container->x;
        }
        if (c->y < container->y) {
            c->h -= (container->y - c->y);
            c->y = container->y;
        }
        if (c->x + c->w > container->x + container->w)
            c->w = container->x + container->w - c->x;
        if (c->y + c->h > container->y + container->h)
            c->h = container->y + container->h - c->y;
        if (c->w <= 0 || c->h <= 0)
            c->visible = 0;
        if (c->child_count > 0 && (c->type == W_VBOX || c->type == W_HBOX || c->type == W_FLEX || c->type == W_LIST ||
                                   c->type == W_GRID))
            xml_ui_clip_children(s, c);
    }
}

void xml_ui_layout(Screen *s, Gfx *g)
{
    if (!s || !g || s->root < 0 || g->w <= 0 || g->h <= 0)
        return;

    Widget *root = &s->widgets[s->root];
    root->x = 0;
    root->y = g->status_h;
    root->w = g->w;
    root->h = g->h - g->status_h - g->softkey_h;
    if (root->h < 1)
        root->h = g->h;
    root->visible = 1;

    for (int i = 0; i < s->count; i++) {
        Widget *w = &s->widgets[i];
        if (w->type == W_STATUSBAR) {
            w->x = 0;
            w->y = 0;
            w->w = g->w;
            w->h = g->status_h;
            w->visible = g->status_h > 0 ? 1 : 0;
        } else if (w->type == W_SOFTKEYS) {
            w->x = 0;
            w->y = g->h - g->softkey_h;
            w->w = g->w;
            w->h = g->softkey_h;
            w->visible = g->softkey_h > 0 ? 1 : 0;
        }
    }

    if (root->type == W_SCREEN || root->type == W_VBOX || root->type == W_HBOX ||
        root->type == W_FLEX || root->type == W_GRID) {
        arrange_widget(s, g, root);
        xml_ui_clip_children(s, root);
        return;
    }

    for (int i = 0; i < root->child_count; i++) {
        Widget *c = &s->widgets[root->children[i]];
        if (is_chrome(c))
            continue;

        measure_widget(s, g, c);

        int rw = xml_ui_resolve_dim(c->sw, c->wu, g->w, g->w, g->w);
        int rh = xml_ui_resolve_dim(c->sh, c->hu, root->h, g->h, g->h);
        int rx = xml_ui_resolve_dim(c->sx, c->xu, g->w, g->w, g->w);
        int ry = xml_ui_resolve_dim(c->sy, c->yu, root->h, g->h, g->h);

        if (c->wu == UNIT_AUTO || c->wu == UNIT_FILL || rw <= 0)
            rw = root->w;
        if (c->hu == UNIT_AUTO || c->hu == UNIT_FILL || rh <= 0)
            rh = root->h;
        if (c->xu == UNIT_AUTO)
            rx = 0;
        if (c->yu == UNIT_AUTO)
            ry = 0;

        c->x = root->x + rx + scale_space(g, c->margin_l);
        c->y = root->y + ry + scale_space(g, c->margin_t);
        c->w = rw - scale_space(g, c->margin_l + c->margin_r);
        c->h = rh - scale_space(g, c->margin_t + c->margin_b);
        if (c->w < 0)
            c->w = 0;
        if (c->h < 0)
            c->h = 0;
        c->visible = 1;

        arrange_widget(s, g, c);
        xml_ui_clip_children(s, c);
    }
}
