#include "xaml_ui.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void xaml_normalize_tag(char *tag)
{
    if (!tag)
        return;
    for (char *p = tag; *p; p++)
        *p = (char)tolower((unsigned char)*p);
}

void xaml_normalize_attr_name(const char *in, char *out, size_t n)
{
    if (!out || n == 0)
        return;
    out[0] = 0;
    if (!in || !in[0])
        return;
    if (str_starts(in, "x:") || str_starts(in, "X:"))
        in += 2;
    char tmp[MAX_ID];
    size_t ti = 0;
    for (const char *p = in; *p && ti + 1 < sizeof(tmp); p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '_' || c == '-') {
            tmp[ti++] = '-';
            continue;
        }
        if (isupper(c) && ti > 0)
            tmp[ti++] = '-';
        tmp[ti++] = (char)tolower(c);
    }
    tmp[ti] = 0;
    if (str_eq(tmp, "name"))
        str_copy(out, n, "id");
    else if (str_eq(tmp, "content"))
        str_copy(out, n, "text");
    else if (str_eq(tmp, "foreground"))
        str_copy(out, n, "color");
    else if (str_eq(tmp, "border-brush"))
        str_copy(out, n, "border-color");
    else if (str_eq(tmp, "border-thickness"))
        str_copy(out, n, "border-width");
    else if (str_eq(tmp, "corner-radius"))
        str_copy(out, n, "radius");
    else if (str_eq(tmp, "orientation"))
        str_copy(out, n, "direction");
    else if (str_eq(tmp, "spacing"))
        str_copy(out, n, "gap");
    else if (str_eq(tmp, "column-spacing"))
        str_copy(out, n, "column-gap");
    else if (str_eq(tmp, "row-spacing"))
        str_copy(out, n, "row-gap");
    else if (str_eq(tmp, "vertical-alignment"))
        str_copy(out, n, "align-items");
    else if (str_eq(tmp, "horizontal-alignment"))
        str_copy(out, n, "justify-content");
    else if (str_eq(tmp, "grid-column-span"))
        str_copy(out, n, "span");
    else if (str_eq(tmp, "grid-row-span"))
        str_copy(out, n, "grid-row");
    else if (str_eq(tmp, "font-size"))
        str_copy(out, n, "font-size");
    else if (str_eq(tmp, "font-weight"))
        str_copy(out, n, "font-weight");
    else
        str_copy(out, n, tmp);
}

const char *xaml_attr(const XmlStart *el, const char *name)
{
    if (!el || !name)
        return NULL;
    const char *direct = xml_attr(el, name);
    if (direct && direct[0])
        return direct;
    char want[MAX_ID];
    xaml_normalize_attr_name(name, want, sizeof(want));
    for (int i = 0; i < el->attr_count; i++) {
        char got[MAX_ID];
        xaml_normalize_attr_name(el->attrs[i].name, got, sizeof(got));
        if (str_eq(got, want) || str_eq(el->attrs[i].name, name))
            return el->attrs[i].value;
    }
    if (str_eq(name, "id")) {
        const char *xn = xml_attr(el, "x:Name");
        if (!xn)
            xn = xml_attr(el, "Name");
        return xn;
    }
    return NULL;
}

WidgetType xaml_tag_type(const char *tag)
{
    char t[MAX_ID];
    str_copy(t, sizeof(t), tag ? tag : "");
    xaml_normalize_tag(t);
    if (str_eq(t, "screen"))
        return W_SCREEN;
    if (str_eq(t, "dashboard") || str_eq(t, "window") || str_eq(t, "taskbar"))
        return W_FLEX;
    if (str_eq(t, "panel") || str_eq(t, "card") || str_eq(t, "dockpanel"))
        return W_FLEX;
    if (str_eq(t, "stack"))
        return W_FLEX;
    if (str_eq(t, "grid"))
        return W_GRID;
    if (str_eq(t, "text") || str_eq(t, "label") || str_eq(t, "header") || str_eq(t, "badge") ||
        str_eq(t, "icon"))
        return W_LABEL;
    if (str_eq(t, "button") || str_eq(t, "row") || str_eq(t, "tile"))
        return W_BUTTON;
    if (str_eq(t, "input"))
        return W_INPUT;
    if (str_eq(t, "list"))
        return W_LIST;
    return xml_ui_tag_type(t);
}

void xaml_apply_attrs(Widget *w, const XmlStart *el, const XmlUiResources *res)
{
    if (!w || !el)
        return;
    XmlStart norm;
    memset(&norm, 0, sizeof(norm));
    str_copy(norm.tag, sizeof(norm.tag), el->tag);
    xaml_normalize_tag(norm.tag);
    norm.self_close = el->self_close;
    for (int i = 0; i < el->attr_count && norm.attr_count < 16; i++) {
        char nname[MAX_ID];
        xaml_normalize_attr_name(el->attrs[i].name, nname, sizeof(nname));
        if (!nname[0] || str_eq(nname, "xmlns"))
            continue;
        str_copy(norm.attrs[norm.attr_count].name, sizeof(norm.attrs[0].name), nname);
        str_copy(norm.attrs[norm.attr_count].value, sizeof(norm.attrs[0].value), el->attrs[i].value);
        norm.attr_count++;
    }
    xml_ui_apply_attrs(w, &norm, res);

    const char *id = xaml_attr(el, "id");
    if (id)
        str_copy(w->id, sizeof(w->id), id);
    const char *content = xaml_attr(el, "Content");
    if (content)
        str_copy(w->text, sizeof(w->text), content);
    const char *title = xaml_attr(el, "Title");
    const char *subtitle = xaml_attr(el, "Subtitle");
    if (title || subtitle) {
        char buf[MAX_TEXT];
        buf[0] = 0;
        if (title)
            str_copy(buf, sizeof(buf), title);
        if (subtitle && subtitle[0]) {
            if (buf[0])
                str_cat(buf, sizeof(buf), "\n");
            str_cat(buf, sizeof(buf), subtitle);
        }
        str_copy(w->text, sizeof(w->text), buf);
    }
    const char *orient = xaml_attr(el, "Orientation");
    if (orient) {
        if (str_eq(orient, "Horizontal") || str_eq(orient, "horizontal") || str_eq(orient, "Row"))
            w->flex_dir = FLEX_ROW;
        else
            w->flex_dir = FLEX_COL;
    }
    const char *span = xaml_attr(el, "GridColumnSpan");
    if (span)
        w->col_span = (int16_t)atoi(span);
    const char *rspan = xaml_attr(el, "GridRowSpan");
    if (rspan)
        w->row_span = (int16_t)atoi(rspan);
    const char *action = xaml_attr(el, "Action");
    if (action && action[0])
        str_copy(w->action, sizeof(w->action), action);

    char tag[MAX_ID];
    str_copy(tag, sizeof(tag), el->tag);
    xaml_normalize_tag(tag);
    if (!w->component[0]) {
        if (str_eq(tag, "card"))
            str_copy(w->component, sizeof(w->component), "card");
        else if (str_eq(tag, "badge"))
            str_copy(w->component, sizeof(w->component), "badge");
        else if (str_eq(tag, "row"))
            str_copy(w->component, sizeof(w->component), "row");
        else if (str_eq(tag, "button"))
            str_copy(w->component, sizeof(w->component), "button");
        else if (str_eq(tag, "text") || str_eq(tag, "header"))
            str_copy(w->component, sizeof(w->component), tag);
        else if (str_eq(tag, "panel") || str_eq(tag, "stack") || str_eq(tag, "dashboard") ||
                 str_eq(tag, "taskbar") || str_eq(tag, "window"))
            str_copy(w->component, sizeof(w->component), "surface");
    }
    if (str_eq(tag, "stack") || str_eq(tag, "panel") || str_eq(tag, "card") ||
        str_eq(tag, "dashboard") || str_eq(tag, "taskbar") || str_eq(tag, "window")) {
        if (w->type == W_FLEX)
            w->type = w->flex_dir == FLEX_ROW ? W_HBOX : W_VBOX;
    }
    if (w->type == W_BUTTON || w->type == W_INPUT)
        w->focusable = 1;
    if (w->bg_set == 0 && (str_eq(tag, "card") || str_eq(tag, "panel") || str_eq(tag, "taskbar"))) {
        w->bg = COL_PANEL;
        w->bg_set = 1;
    }
}
