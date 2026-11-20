#include "xml_ui.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifndef _WIN32
#include <dirent.h>
#endif

#define CSS_MAX_FILE 65536
#define CSS_MAX_SELECTORS 8
#define CSS_MAX_DIR_FILES 24

static char *css_trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    return s;
}

static char *css_read(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    char *raw = (char *)malloc(CSS_MAX_FILE + 1);
    if (!raw) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(raw, 1, CSS_MAX_FILE, f);
    fclose(f);
    raw[n] = 0;
    size_t o = 0;
    size_t i = 0;
    while (i < n) {
        if (raw[i] == '/' && i + 1 < n && raw[i + 1] == '*') {
            i += 2;
            while (i < n && !(raw[i] == '*' && i + 1 < n && raw[i + 1] == '/'))
                i++;
            i = (i + 2 <= n) ? i + 2 : n;
            continue;
        }
        raw[o++] = raw[i++];
    }
    raw[o] = 0;
    return raw;
}

static XmlUiStyle *style_slot(XmlUiResources *r, const char *name)
{
    for (int i = 0; i < r->style_count; i++) {
        if (str_eq(r->styles[i].name, name))
            return &r->styles[i];
    }
    if (r->style_count >= XML_UI_MAX_STYLES)
        return NULL;
    XmlUiStyle *st = &r->styles[r->style_count++];
    memset(st, 0, sizeof(*st));
    str_copy(st->name, sizeof(st->name), name);
    return st;
}

static void style_set(XmlUiStyle *st, const char *name, const char *value)
{
    char key[MAX_ID];
    size_t n = 0;
    for (; name[n] && n + 1 < sizeof(key); n++)
        key[n] = (char)tolower((unsigned char)name[n]);
    key[n] = 0;
    for (int i = 0; i < st->prop_count; i++) {
        if (str_eq(st->props[i].name, key)) {
            str_copy(st->props[i].value, sizeof(st->props[i].value), value);
            return;
        }
    }
    if (st->prop_count >= XML_UI_STYLE_PROPS)
        return;
    str_copy(st->props[st->prop_count].name, sizeof(st->props[st->prop_count].name), key);
    str_copy(st->props[st->prop_count].value, sizeof(st->props[st->prop_count].value), value);
    st->prop_count++;
}

static int css_class_name(char *sel, char *out, size_t out_sz)
{
    sel = css_trim(sel);
    if (sel[0] != '.')
        return 0;
    size_t n = 0;
    const char *p = sel + 1;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '-')) {
        if (n + 1 < out_sz)
            out[n++] = *p;
        p++;
    }
    out[n] = 0;
    return n > 0 && *p == 0;
}

static void css_apply_rule(XmlUiResources *r, char *selectors, char *body)
{
    XmlUiStyle *slots[CSS_MAX_SELECTORS];
    int ns = 0;
    char *s = selectors;
    while (s && *s && ns < CSS_MAX_SELECTORS) {
        char *comma = strchr(s, ',');
        if (comma)
            *comma = 0;
        char name[MAX_ID];
        if (css_class_name(s, name, sizeof(name))) {
            XmlUiStyle *st = style_slot(r, name);
            if (st)
                slots[ns++] = st;
        }
        s = comma ? comma + 1 : NULL;
    }
    if (ns == 0)
        return;
    char *d = body;
    while (d && *d) {
        char *semi = strchr(d, ';');
        if (semi)
            *semi = 0;
        char *colon = strchr(d, ':');
        if (colon) {
            *colon = 0;
            char *name = css_trim(d);
            char *value = css_trim(colon + 1);
            if (name[0] && value[0]) {
                for (int i = 0; i < ns; i++)
                    style_set(slots[i], name, value);
            }
        }
        d = semi ? semi + 1 : NULL;
    }
}

int xml_ui_style_load_file(XmlUiResources *r, const char *path)
{
    if (!r || !path)
        return -1;
    char *buf = css_read(path);
    if (!buf)
        return -1;
    char *p = buf;
    while (*p) {
        char *open = strchr(p, '{');
        if (!open)
            break;
        char *close = strchr(open + 1, '}');
        if (!close)
            break;
        *open = 0;
        *close = 0;
        char *selectors = p;
        char *body = open + 1;
        p = close + 1;
        css_apply_rule(r, selectors, body);
    }
    free(buf);
    return 0;
}

#ifndef _WIN32
static int css_name_cmp(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}
#endif

int xml_ui_style_load_dir(XmlUiResources *r, const char *dir)
{
    if (!r || !dir || !dir[0])
        return -1;
    char path[MAX_PATH];
#ifndef _WIN32
    DIR *d = opendir(dir);
    if (!d)
        return -1;
    char names[CSS_MAX_DIR_FILES][MAX_ID];
    int count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && count < CSS_MAX_DIR_FILES) {
        size_t n = strlen(de->d_name);
        if (n < 5 || n >= MAX_ID || !str_eq(de->d_name + n - 4, ".css"))
            continue;
        str_copy(names[count], sizeof(names[count]), de->d_name);
        count++;
    }
    closedir(d);
    qsort(names, (size_t)count, sizeof(names[0]), css_name_cmp);
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        xml_ui_style_load_file(r, path);
    }
    return 0;
#else
    snprintf(path, sizeof(path), "%s/theme.css", dir);
    return xml_ui_style_load_file(r, path);
#endif
}

const XmlUiStyle *xml_ui_style_find(const XmlUiResources *r, const char *name)
{
    if (!r || !name)
        return NULL;
    for (int i = 0; i < r->style_count; i++) {
        if (str_eq(r->styles[i].name, name))
            return &r->styles[i];
    }
    return NULL;
}

static int parse_justify_v(const char *s)
{
    if (!s)
        return JUSTIFY_START;
    if (str_eq(s, "center"))
        return JUSTIFY_CENTER;
    if (str_eq(s, "end") || str_eq(s, "flex-end"))
        return JUSTIFY_END;
    if (str_eq(s, "between") || str_eq(s, "space-between"))
        return JUSTIFY_BETWEEN;
    return JUSTIFY_START;
}

static int parse_align_v(const char *s)
{
    if (!s)
        return ALIGN_STRETCH;
    if (str_eq(s, "center"))
        return ALIGN_CENTER;
    if (str_eq(s, "end") || str_eq(s, "flex-end"))
        return ALIGN_END;
    if (str_eq(s, "start") || str_eq(s, "flex-start"))
        return ALIGN_START;
    return ALIGN_STRETCH;
}

static int first_int(const char *s, int *out)
{
    while (*s && !isdigit((unsigned char)*s))
        s++;
    if (!*s)
        return 0;
    *out = atoi(s);
    return 1;
}

static int is_flex_container(const Widget *w)
{
    return w->type == W_VBOX || w->type == W_HBOX || w->type == W_FLEX;
}

static void set_dir(Widget *w, int dir)
{
    w->flex_dir = (uint8_t)dir;
    if (is_flex_container(w))
        w->type = dir == FLEX_ROW ? W_HBOX : W_VBOX;
}

static void set_dim(Widget *w, const XmlUiResources *res, int which, const char *v)
{
    int out = 0;
    int u = UNIT_AUTO;
    if (xml_ui_res_resolve_dim(res, v, &out, &u) != 0) {
        out = 0;
        u = UNIT_AUTO;
    }
    switch (which) {
    case 0:
        w->sx = out;
        w->xu = (unsigned)u;
        break;
    case 1:
        w->sy = out;
        w->yu = (unsigned)u;
        break;
    case 2:
        w->sw = out;
        w->wu = (unsigned)u;
        break;
    default:
        w->sh = out;
        w->hu = (unsigned)u;
        break;
    }
}

static int color_prop(const XmlUiResources *res, const char *v, uint16_t *out)
{
    uint16_t c = *out;
    if (!v || !v[0])
        return 0;
    if (xml_ui_res_resolve_color(res, v, &c, *out) != 0)
        return 0;
    *out = c;
    return 1;
}

static void set_border(Widget *w, const XmlUiResources *res, const char *v)
{
    char buf[MAX_ATTR];
    str_copy(buf, sizeof(buf), v);
    char *p = buf;
    while (*p) {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        char *tok = p;
        while (*p && !isspace((unsigned char)*p))
            p++;
        if (*p)
            *p++ = 0;
        if (str_eq(tok, "none"))
            w->border = 0;
        else if (isdigit((unsigned char)tok[0]))
            w->border = (int16_t)atoi(tok);
        else if (tok[0] == '#' || tok[0] == '@')
            color_prop(res, tok, &w->border_c);
    }
}

static int grid_columns(const char *v)
{
    int n = 0;
    const char *rep = strstr(v, "repeat");
    if (rep) {
        first_int(rep + 6, &n);
        return n;
    }
    int tokens = 0;
    const char *q = v;
    while (*q) {
        while (*q && (isspace((unsigned char)*q) || *q == ','))
            q++;
        if (!*q)
            break;
        tokens++;
        while (*q && !isspace((unsigned char)*q) && *q != ',')
            q++;
    }
    if (tokens > 1)
        return tokens;
    first_int(v, &n);
    return n;
}

static int span_value(const char *v)
{
    int n = 1;
    const char *sp = strstr(v, "span");
    if (sp)
        first_int(sp + 4, &n);
    else
        first_int(v, &n);
    return n < 1 ? 1 : n;
}

static int truthy(const char *v)
{
    return str_eq(v, "true") || str_eq(v, "1") || str_eq(v, "wrap") || str_eq(v, "yes");
}

void xml_ui_style_apply_prop(Widget *w, const char *name, const char *value, const XmlUiResources *res)
{
    if (!w || !name || !value)
        return;

    if (str_eq(name, "x"))
        set_dim(w, res, 0, value);
    else if (str_eq(name, "y"))
        set_dim(w, res, 1, value);
    else if (str_eq(name, "width"))
        set_dim(w, res, 2, value);
    else if (str_eq(name, "height"))
        set_dim(w, res, 3, value);
    else if (str_eq(name, "background") || str_eq(name, "background-color")) {
        if (str_eq(value, "transparent") || str_eq(value, "none")) {
            w->bg_set = 0;
        } else if (color_prop(res, value, &w->bg)) {
            w->bg_set = 1;
        }
    } else if (str_eq(name, "color")) {
        if (color_prop(res, value, &w->fg))
            w->fg_set = 1;
    } else if (str_eq(name, "border-color"))
        color_prop(res, value, &w->border_c);
    else if (str_eq(name, "margin"))
        xml_ui_parse_spacing(value, &w->margin_t, &w->margin_r, &w->margin_b, &w->margin_l);
    else if (str_eq(name, "margin-top"))
        w->margin_t = (int16_t)atoi(value);
    else if (str_eq(name, "margin-right"))
        w->margin_r = (int16_t)atoi(value);
    else if (str_eq(name, "margin-bottom"))
        w->margin_b = (int16_t)atoi(value);
    else if (str_eq(name, "margin-left"))
        w->margin_l = (int16_t)atoi(value);
    else if (str_eq(name, "padding"))
        xml_ui_parse_spacing(value, &w->pad_t, &w->pad_r, &w->pad_b, &w->pad_l);
    else if (str_eq(name, "padding-top"))
        w->pad_t = (int16_t)atoi(value);
    else if (str_eq(name, "padding-right"))
        w->pad_r = (int16_t)atoi(value);
    else if (str_eq(name, "padding-bottom"))
        w->pad_b = (int16_t)atoi(value);
    else if (str_eq(name, "padding-left"))
        w->pad_l = (int16_t)atoi(value);
    else if (str_eq(name, "gap")) {
        w->gap = (int16_t)atoi(value);
        w->row_gap = w->gap;
        w->col_gap = w->gap;
    } else if (str_eq(name, "row-gap"))
        w->row_gap = (int16_t)atoi(value);
    else if (str_eq(name, "col-gap") || str_eq(name, "column-gap"))
        w->col_gap = (int16_t)atoi(value);
    else if (str_eq(name, "border"))
        set_border(w, res, value);
    else if (str_eq(name, "border-width"))
        w->border = (int16_t)atoi(value);
    else if (str_eq(name, "radius") || str_eq(name, "border-radius"))
        w->radius = (int16_t)atoi(value);
    else if (str_eq(name, "min-width"))
        w->min_w = (int16_t)atoi(value);
    else if (str_eq(name, "min-height"))
        w->min_h = (int16_t)atoi(value);
    else if (str_eq(name, "max-width"))
        w->max_w = (int16_t)atoi(value);
    else if (str_eq(name, "max-height"))
        w->max_h = (int16_t)atoi(value);
    else if (str_eq(name, "flex")) {
        int g = 0;
        if (first_int(value, &g))
            w->grow = (int16_t)g;
    } else if (str_eq(name, "grow") || str_eq(name, "flex-grow"))
        w->grow = (int16_t)atoi(value);
    else if (str_eq(name, "shrink") || str_eq(name, "flex-shrink"))
        w->shrink = (int16_t)atoi(value);
    else if (str_eq(name, "basis") || str_eq(name, "flex-basis")) {
        if (w->wu == UNIT_AUTO && w->hu == UNIT_AUTO) {
            int bv = 0;
            int bu = UNIT_AUTO;
            if (xml_ui_res_resolve_dim(res, value, &bv, &bu) == 0) {
                w->sw = bv;
                w->wu = (unsigned)bu;
            }
        }
    } else if (str_eq(name, "direction") || str_eq(name, "flex-direction") || str_eq(name, "layout")) {
        if (str_eq(value, "row") || str_eq(value, "horizontal"))
            set_dir(w, FLEX_ROW);
        else if (str_eq(value, "column") || str_eq(value, "vertical"))
            set_dir(w, FLEX_COL);
    } else if (str_eq(name, "justify") || str_eq(name, "justify-content"))
        w->justify = (uint8_t)parse_justify_v(value);
    else if (str_eq(name, "align") || str_eq(name, "align-items"))
        w->align = (uint8_t)parse_align_v(value);
    else if (str_eq(name, "wrap") || str_eq(name, "flex-wrap"))
        w->wrap = truthy(value) ? 1 : 0;
    else if (str_eq(name, "columns") || str_eq(name, "grid-template-columns")) {
        int c = grid_columns(value);
        if (c > 0)
            w->grid_cols = (int16_t)c;
        {
            const char *star = strchr(value, '*');
            if (star) {
                const char *p = star + 1;
                while (*p && (isspace((unsigned char)*p) || *p == ','))
                    p++;
                int px = 0;
                if (first_int(p, &px) && px > 0)
                    w->max_w = (int16_t)px;
            }
        }
    } else if (str_eq(name, "rows") || str_eq(name, "grid-template-rows")) {
        int rws = 0;
        if (first_int(value, &rws))
            w->grid_rows = (int16_t)rws;
    } else if (str_eq(name, "span") || str_eq(name, "grid-column-span"))
        w->col_span = (int16_t)span_value(value);
    else if (str_eq(name, "grid-row-span"))
        w->row_span = (int16_t)span_value(value);
    else if (str_eq(name, "display")) {
        if (str_eq(value, "flex")) {
            if (w->type == W_FLEX)
                w->type = w->flex_dir == FLEX_ROW ? W_HBOX : W_VBOX;
        } else if (str_eq(value, "grid")) {
            if (is_flex_container(w))
                w->type = W_GRID;
        }
    } else if (str_eq(name, "overflow")) {
        if (str_eq(value, "hidden"))
            w->overflow = 1;
        else if (str_eq(value, "scroll") || str_eq(value, "auto"))
            w->overflow = 2;
        else if (str_eq(value, "ellipsis"))
            w->overflow = 3;
        else
            w->overflow = 0;
    } else if (str_eq(name, "text-align")) {
        if (str_eq(value, "center"))
            w->text_align = 1;
        else if (str_eq(value, "right") || str_eq(value, "end"))
            w->text_align = 2;
        else
            w->text_align = 0;
    } else if (str_eq(name, "align-text"))
        w->align = (uint8_t)parse_align_v(value);
    else if (str_eq(name, "icon")) {
        if (value[0])
            str_copy(w->icon, sizeof(w->icon), value);
    } else if (str_eq(name, "icon-set")) {
        if (value[0])
            str_copy(w->icon_set, sizeof(w->icon_set), value);
    } else if (str_eq(name, "font-family") || str_eq(name, "font")) {
        if (value[0])
            str_copy(w->font_family, sizeof(w->font_family), value);
    } else if (str_eq(name, "font-size")) {
        int px = atoi(value);
        if (px > 0)
            w->font_size = (int16_t)px;
    } else if (str_eq(name, "font-weight")) {
        if (str_eq(value, "bold") || str_eq(value, "700"))
            w->font_weight = 700;
        else if (str_eq(value, "semibold") || str_eq(value, "600"))
            w->font_weight = 600;
        else if (str_eq(value, "medium") || str_eq(value, "500"))
            w->font_weight = 500;
        else if (str_eq(value, "light") || str_eq(value, "300"))
            w->font_weight = 300;
        else {
            int wv = atoi(value);
            if (wv > 0)
                w->font_weight = (int16_t)wv;
        }
    }
}

static int prop_pass(const char *n)
{
    if (str_eq(n, "padding") || str_eq(n, "margin") || str_eq(n, "gap") || str_eq(n, "border") ||
        str_eq(n, "flex"))
        return 0;
    if (str_eq(n, "basis") || str_eq(n, "flex-basis"))
        return 2;
    if (str_eq(n, "display"))
        return 3;
    return 1;
}

void xml_ui_style_apply_props(Widget *w, const char *const *names, const char *const *values, int n,
                              const XmlUiResources *res)
{
    if (!w || !names || !values)
        return;
    for (int pass = 0; pass < 4; pass++) {
        for (int i = 0; i < n; i++) {
            if (!names[i] || !values[i])
                continue;
            if (prop_pass(names[i]) == pass)
                xml_ui_style_apply_prop(w, names[i], values[i], res);
        }
    }
    if ((w->wu == UNIT_FILL || w->hu == UNIT_FILL) && w->grow <= 0)
        w->grow = 1;
}

static int digits_only(const char *s)
{
    if (!*s)
        return 0;
    for (; *s; s++) {
        if (!isdigit((unsigned char)*s))
            return 0;
    }
    return 1;
}

static int prefixed_num(const char *tok, const char *pfx, int *out)
{
    if (!str_starts(tok, pfx))
        return 0;
    const char *d = tok + strlen(pfx);
    if (!digits_only(d))
        return 0;
    *out = atoi(d);
    return 1;
}

static void set_surface(Widget *w)
{
    w->bg = COL_PANEL;
    w->bg_set = 1;
    w->radius = 12;
    w->border = 1;
    w->border_c = COL_BORDER;
}

static int apply_builtin(Widget *w, const char *t)
{
    int v = 0;
    if (str_eq(t, "page")) {
        w->bg = COL_BG;
        w->bg_set = 1;
        w->sw = 0;
        w->sh = 0;
        w->wu = UNIT_FILL;
        w->hu = UNIT_FILL;
        if (w->grow <= 0)
            w->grow = 1;
        w->flex_dir = FLEX_COL;
        if (w->type == W_FLEX || w->type == W_HBOX)
            w->type = W_VBOX;
        return 1;
    }
    if (str_eq(t, "col")) {
        set_dir(w, FLEX_COL);
        return 1;
    }
    if (str_eq(t, "row")) {
        set_dir(w, FLEX_ROW);
        return 1;
    }
    if (prefixed_num(t, "gap-", &v)) {
        w->gap = (int16_t)v;
        w->row_gap = (int16_t)v;
        w->col_gap = (int16_t)v;
        return 1;
    }
    if (prefixed_num(t, "p-", &v)) {
        w->pad_t = w->pad_r = w->pad_b = w->pad_l = (int16_t)v;
        return 1;
    }
    if (prefixed_num(t, "px-", &v)) {
        w->pad_l = w->pad_r = (int16_t)v;
        return 1;
    }
    if (prefixed_num(t, "py-", &v)) {
        w->pad_t = w->pad_b = (int16_t)v;
        return 1;
    }
    if (prefixed_num(t, "pt-", &v)) {
        w->pad_t = (int16_t)v;
        return 1;
    }
    if (prefixed_num(t, "pr-", &v)) {
        w->pad_r = (int16_t)v;
        return 1;
    }
    if (prefixed_num(t, "pb-", &v)) {
        w->pad_b = (int16_t)v;
        return 1;
    }
    if (prefixed_num(t, "pl-", &v)) {
        w->pad_l = (int16_t)v;
        return 1;
    }
    if (str_eq(t, "items-center")) {
        w->align = ALIGN_CENTER;
        return 1;
    }
    if (str_eq(t, "items-start")) {
        w->align = ALIGN_START;
        return 1;
    }
    if (str_eq(t, "items-end")) {
        w->align = ALIGN_END;
        return 1;
    }
    if (str_eq(t, "items-stretch")) {
        w->align = ALIGN_STRETCH;
        return 1;
    }
    if (str_eq(t, "justify-start")) {
        w->justify = JUSTIFY_START;
        return 1;
    }
    if (str_eq(t, "justify-center")) {
        w->justify = JUSTIFY_CENTER;
        return 1;
    }
    if (str_eq(t, "justify-end")) {
        w->justify = JUSTIFY_END;
        return 1;
    }
    if (str_eq(t, "justify-between")) {
        w->justify = JUSTIFY_BETWEEN;
        return 1;
    }
    if (str_eq(t, "surface")) {
        set_surface(w);
        return 1;
    }
    if (str_eq(t, "card")) {
        set_surface(w);
        w->pad_t = w->pad_r = w->pad_b = w->pad_l = 14;
        return 1;
    }
    if (str_eq(t, "text-primary")) {
        w->fg = COL_TEXT;
        w->fg_set = 1;
        return 1;
    }
    if (str_eq(t, "text-secondary")) {
        w->fg = COL_DIM;
        w->fg_set = 1;
        return 1;
    }
    if (str_eq(t, "rounded")) {
        w->radius = 10;
        return 1;
    }
    if (str_eq(t, "rounded-lg")) {
        w->radius = 14;
        return 1;
    }
    return 0;
}

void xml_ui_style_apply_class(Widget *w, const char *classes, const XmlUiResources *res)
{
    if (!w || !classes || !classes[0])
        return;
    char buf[MAX_ATTR * 2];
    str_copy(buf, sizeof(buf), classes);
    char *p = buf;
    while (*p) {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        char *tok = p;
        while (*p && !isspace((unsigned char)*p))
            p++;
        if (*p)
            *p++ = 0;
        apply_builtin(w, tok);
        const XmlUiStyle *st = xml_ui_style_find(res, tok);
        if (st) {
            const char *names[XML_UI_STYLE_PROPS];
            const char *values[XML_UI_STYLE_PROPS];
            for (int i = 0; i < st->prop_count; i++) {
                names[i] = st->props[i].name;
                values[i] = st->props[i].value;
            }
            xml_ui_style_apply_props(w, names, values, st->prop_count, res);
        }
    }
}

void xml_ui_style_apply_variant(Widget *w, const char *variant)
{
    if (!w || !variant || !variant[0])
        return;
    if (str_eq(variant, "primary")) {
        w->bg = COL_ACCENT;
        w->bg_set = 1;
        w->fg = COL_ON_ACCENT;
        w->fg_set = 1;
        if (w->radius <= 0)
            w->radius = 12;
    } else if (str_eq(variant, "secondary")) {
        w->bg = COL_PANEL;
        w->bg_set = 1;
        w->fg = COL_TEXT;
        w->fg_set = 1;
        if (w->radius <= 0)
            w->radius = 10;
    } else if (str_eq(variant, "ghost")) {
        w->bg = COL_BG;
        w->bg_set = 1;
        w->fg = COL_ACCENT;
        w->fg_set = 1;
        w->border = 1;
        w->border_c = COL_ACCENT;
        if (w->radius <= 0)
            w->radius = 10;
    }
}
