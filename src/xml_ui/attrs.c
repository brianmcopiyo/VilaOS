#include "xml_ui.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

WidgetType xml_ui_tag_type(const char *tag)
{
    if (!tag)
        return W_LABEL;
    if (str_eq(tag, "screen"))
        return W_SCREEN;
    if (str_eq(tag, "vbox") || str_eq(tag, "column"))
        return W_VBOX;
    if (str_eq(tag, "hbox"))
        return W_HBOX;
    if (str_eq(tag, "flex"))
        return W_FLEX;
    if (str_eq(tag, "grid"))
        return W_GRID;
    if (str_eq(tag, "label") || str_eq(tag, "text") || str_eq(tag, "header") ||
        str_eq(tag, "badge") || str_eq(tag, "icon"))
        return W_LABEL;
    if (str_eq(tag, "button") || str_eq(tag, "tile") || str_eq(tag, "row") || str_eq(tag, "dock"))
        return W_BUTTON;
    if (str_eq(tag, "input"))
        return W_INPUT;
    if (str_eq(tag, "list"))
        return W_LIST;
    if (str_eq(tag, "statusbar"))
        return W_STATUSBAR;
    if (str_eq(tag, "softkeys") || str_eq(tag, "footer"))
        return W_SOFTKEYS;
    if (str_eq(tag, "component"))
        return W_COMPONENT;
    return W_LABEL;
}

void xml_ui_parse_spacing(const char *spec, int16_t *t, int16_t *r, int16_t *b, int16_t *l)
{
    int vals[4] = {0, 0, 0, 0};
    int n = 0;
    if (!spec || !spec[0]) {
        if (t)
            *t = 0;
        if (r)
            *r = 0;
        if (b)
            *b = 0;
        if (l)
            *l = 0;
        return;
    }
    const char *p = spec;
    while (*p && n < 4) {
        while (*p && (isspace((unsigned char)*p) || *p == ','))
            p++;
        if (!*p)
            break;
        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (end == p)
            break;
        vals[n++] = (int)v;
        p = end;
        while (*p && isalpha((unsigned char)*p))
            p++;
    }
    if (n == 1) {
        vals[1] = vals[2] = vals[3] = vals[0];
    } else if (n == 2) {
        vals[2] = vals[0];
        vals[3] = vals[1];
    } else if (n == 3) {
        vals[3] = vals[1];
    }
    if (t)
        *t = (int16_t)vals[0];
    if (r)
        *r = (int16_t)vals[1];
    if (b)
        *b = (int16_t)vals[2];
    if (l)
        *l = (int16_t)vals[3];
}

static int parse_bool(const char *s)
{
    if (!s || !s[0])
        return -1;
    if (str_eq(s, "true") || str_eq(s, "1") || str_eq(s, "yes"))
        return 1;
    if (str_eq(s, "false") || str_eq(s, "0") || str_eq(s, "no"))
        return 0;
    return -1;
}

static const char *attr2(const XmlStart *el, const char *a, const char *b)
{
    const char *v = xml_attr(el, a);
    if (v && v[0])
        return v;
    return b ? xml_attr(el, b) : NULL;
}

static void combine_title(Widget *w, const char *title, const char *subtitle)
{
    char buf[MAX_TEXT];
    buf[0] = 0;
    if (title && title[0])
        str_copy(buf, sizeof(buf), title);
    if (subtitle && subtitle[0]) {
        if (buf[0])
            str_cat(buf, sizeof(buf), "\n");
        str_cat(buf, sizeof(buf), subtitle);
    }
    str_copy(w->text, sizeof(w->text), buf);
}

void xml_ui_apply_attrs(Widget *w, const XmlStart *el, const XmlUiResources *res)
{
    if (!w || !el)
        return;

    const char *cls = xml_attr(el, "class");
    if (cls && cls[0])
        xml_ui_style_apply_class(w, cls, res);
    const char *variant = xml_attr(el, "variant");
    if (variant && variant[0])
        xml_ui_style_apply_variant(w, variant);

    const char *names[16];
    const char *values[16];
    int pn = 0;
    for (int i = 0; i < el->attr_count && pn < 16; i++) {
        names[pn] = el->attrs[i].name;
        values[pn] = el->attrs[i].value;
        pn++;
    }
    xml_ui_style_apply_props(w, names, values, pn, res);

    const char *id = xml_attr(el, "id");
    if (id)
        str_copy(w->id, sizeof(w->id), id);

    const char *text = xml_attr(el, "text");
    if (text)
        str_copy(w->text, sizeof(w->text), text);
    const char *title = xml_attr(el, "title");
    const char *subtitle = xml_attr(el, "subtitle");
    if (!text && ((title && title[0]) || (subtitle && subtitle[0])))
        combine_title(w, title, subtitle);
    const char *hint = attr2(el, "hint", "placeholder");
    if (hint && !w->text[0]) {
        str_copy(w->text, sizeof(w->text), hint);
        if (w->type == W_INPUT)
            w->value = 1;
    }

    const char *action = attr2(el, "action", "on-click");
    const char *route = xml_attr(el, "route");
    if (action && action[0]) {
        str_copy(w->action, sizeof(w->action), action);
    } else if (route && route[0]) {
        if (str_starts(route, "route:") || str_starts(route, "app:") || str_starts(route, "assets/")) {
            str_copy(w->action, sizeof(w->action), route);
        } else {
            char buf[MAX_PATH];
            snprintf(buf, sizeof(buf), "route:push:%s", route);
            str_copy(w->action, sizeof(w->action), buf);
        }
    }

    const char *icon = xml_attr(el, "icon");
    if (icon)
        str_copy(w->icon, sizeof(w->icon), icon);
    const char *iset = xml_attr(el, "icon-set");
    if (iset)
        str_copy(w->icon_set, sizeof(w->icon_set), iset);
    const char *comp = xml_attr(el, "name");
    if (!comp)
        comp = xml_attr(el, "component");
    if (comp)
        str_copy(w->component, sizeof(w->component), comp);

    if (w->type == W_HBOX)
        w->flex_dir = FLEX_ROW;
    else if (w->type == W_VBOX)
        w->flex_dir = FLEX_COL;

    const char *scroll = xml_attr(el, "scroll");
    if (scroll) {
        int b = parse_bool(scroll);
        if (b == 1 || b == 0)
            w->scroll = 0;
        else
            w->scroll = atoi(scroll);
    }

    int vis = parse_bool(xml_attr(el, "visible"));
    if (vis >= 0)
        w->visible = vis;
    int en = parse_bool(xml_attr(el, "enabled"));
    if (en == 0)
        w->focusable = 0;
    int foc = parse_bool(xml_attr(el, "focused"));
    if (foc == 1)
        w->focused = 1;
    int sel = parse_bool(attr2(el, "selected", "checked"));
    if (sel == 1)
        w->list_sel = 1;

    const char *val = xml_attr(el, "value");
    if (val)
        w->value = atoi(val);

    const char *onchange = xml_attr(el, "on-change");
    const char *onfocus = xml_attr(el, "on-focus");
    const char *onsubmit = xml_attr(el, "on-submit");
    if ((!w->action[0] || str_eq(w->action, "none")) && onsubmit && onsubmit[0])
        str_copy(w->action, sizeof(w->action), onsubmit);
    else if ((!w->action[0] || str_eq(w->action, "none")) && onchange && onchange[0])
        str_copy(w->action, sizeof(w->action), onchange);
    else if ((!w->action[0] || str_eq(w->action, "none")) && onfocus && onfocus[0])
        str_copy(w->action, sizeof(w->action), onfocus);
}
