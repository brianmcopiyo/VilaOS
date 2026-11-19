#include "xml_ui.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    Screen *s;
    XmlUiResources *res;
    int stack[32];
    int sp;
    int in_component;
} BuildCtx;

static void attach_child(Screen *s, int parent, int child)
{
    if (parent < 0 || child < 0 || parent >= s->count || child >= s->count)
        return;
    Widget *p = &s->widgets[parent];
    if (p->child_count < MAX_CHILDREN)
        p->children[p->child_count++] = child;
}

static int push_widget(BuildCtx *ctx, WidgetType type)
{
    Screen *s = ctx->s;
    if (s->count >= MAX_WIDGETS)
        return -1;
    int idx = s->count++;
    Widget *w = &s->widgets[idx];
    memset(w, 0, sizeof(*w));
    w->type = type;
    w->parent = ctx->sp > 0 ? ctx->stack[ctx->sp - 1] : -1;
    w->visible = 1;
    w->bg = COL_PANEL;
    w->fg = COL_TEXT;
    w->focus_bg = COL_FOCUS;
    w->border_c = COL_BORDER;
    w->max_len = MAX_TEXT - 1;
    w->xu = w->yu = w->wu = w->hu = UNIT_AUTO;
    w->pad_t = w->pad_r = w->pad_b = w->pad_l = -1;
    w->flex_dir = type == W_HBOX ? FLEX_ROW : FLEX_COL;
    w->align = ALIGN_STRETCH;
    w->col_span = 1;
    w->row_span = 1;
    w->shrink = 1;
    if (w->parent >= 0)
        attach_child(s, w->parent, idx);
    else if (type == W_SCREEN)
        s->root = idx;
    return idx;
}

static int expand_component(BuildCtx *ctx, const char *name, const XmlStart *overrides);

static int is_primitive_tag(const char *tag)
{
    static const char *const tags[] = {"screen", "vbox", "hbox", "column", "flex", "grid",
                                       "label", "button", "input", "list", "statusbar", "softkeys"};
    for (size_t i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
        if (str_eq(tag, tags[i]))
            return 1;
    }
    return 0;
}

static int is_container_type(WidgetType t)
{
    return t == W_VBOX || t == W_HBOX || t == W_FLEX || t == W_GRID;
}

static int component_known(const BuildCtx *ctx, const char *name)
{
    if (!ctx->res)
        return 0;
    for (int i = 0; i < ctx->res->component_count; i++) {
        if (str_eq(name, ctx->res->components[i].name))
            return 1;
    }
    return 0;
}

static void on_start(const XmlStart *el, void *user)
{
    BuildCtx *ctx = (BuildCtx *)user;
    Screen *s = ctx->s;
    if (s->count >= MAX_WIDGETS)
        return;

    if (str_eq(el->tag, "include") || str_eq(el->tag, "component")) {
        const char *name = xml_attr(el, "name");
        if (!name)
            name = xml_attr(el, "component");
        if (name && name[0]) {
            int idx = expand_component(ctx, name, el);
            if (idx >= 0 && !el->self_close && ctx->sp < 32)
                ctx->stack[ctx->sp++] = idx;
        }
        return;
    }

    if (ctx->res && !(ctx->in_component > 0 && is_primitive_tag(el->tag)) && component_known(ctx, el->tag)) {
        int idx = expand_component(ctx, el->tag, el);
        if (idx >= 0 && !el->self_close && ctx->sp < 32)
            ctx->stack[ctx->sp++] = idx;
        return;
    }

    WidgetType type = xml_ui_tag_type(el->tag);
    int idx = push_widget(ctx, type);
    if (idx < 0)
        return;
    Widget *w = &s->widgets[idx];
    xml_ui_apply_attrs(w, el, ctx->res);

    if (w->type == W_BUTTON || w->type == W_INPUT)
        w->focusable = 1;
    if (w->type == W_FLEX)
        w->type = w->flex_dir == FLEX_ROW ? W_HBOX : W_VBOX;

    if (w->type == W_SOFTKEYS) {
        const char *l = xml_attr(el, "left");
        const char *c = xml_attr(el, "center");
        const char *r = xml_attr(el, "right");
        const char *la = xml_attr(el, "left-action");
        const char *ca = xml_attr(el, "center-action");
        const char *ra = xml_attr(el, "right-action");
        if (!la)
            la = xml_attr(el, "on-click");
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

    if (!el->self_close && ctx->sp < 32)
        ctx->stack[ctx->sp++] = idx;
}

static void on_end(const char *tag, void *user)
{
    BuildCtx *ctx = (BuildCtx *)user;
    if (ctx->sp <= 0)
        return;
    Widget *top = &ctx->s->widgets[ctx->stack[ctx->sp - 1]];
    if (tag && tag[0]) {
        WidgetType t = xml_ui_tag_type(tag);
        int both_containers = is_container_type(t) && is_container_type(top->type);
        if (t != top->type && !both_containers && !str_eq(tag, "component") && !str_eq(tag, "include")) {
            if (!component_known(ctx, tag))
                return;
        }
    }
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

typedef struct {
    BuildCtx *host;
    int root_idx;
    int remapped;
} CompCtx;

static void comp_start(const XmlStart *el, void *user)
{
    CompCtx *cc = (CompCtx *)user;
    BuildCtx *ctx = cc->host;
    if (str_eq(el->tag, "component") || str_eq(el->tag, "template")) {
        xml_ui_apply_attrs(&ctx->s->widgets[cc->root_idx], el, ctx->res);
        if (!el->self_close && ctx->sp < 32)
            ctx->stack[ctx->sp++] = cc->root_idx;
        return;
    }
    on_start(el, ctx);
}

static void comp_end(const char *tag, void *user)
{
    CompCtx *cc = (CompCtx *)user;
    if (str_eq(tag, "component") || str_eq(tag, "template")) {
        if (cc->host->sp > 0)
            cc->host->sp--;
        return;
    }
    on_end(tag, cc->host);
}

static void transfer_to_leaf(Widget *w, Widget *leaf, const char *name)
{
    if (w->id[0]) {
        str_copy(leaf->id, sizeof(leaf->id), w->id);
        w->id[0] = 0;
    }
    if (w->text[0])
        str_copy(leaf->text, sizeof(leaf->text), w->text);
    if (w->action[0])
        str_copy(leaf->action, sizeof(leaf->action), w->action);
    if (w->icon[0])
        str_copy(leaf->icon, sizeof(leaf->icon), w->icon);
    if (w->value)
        leaf->value = w->value;
    if (!leaf->component[0])
        str_copy(leaf->component, sizeof(leaf->component), name);
    if (w->bg_set) {
        leaf->bg = w->bg;
        leaf->bg_set = 1;
        w->bg_set = 0;
    }
    if (w->fg_set) {
        leaf->fg = w->fg;
        leaf->fg_set = 1;
        w->fg_set = 0;
    }
    if (w->radius > 0) {
        leaf->radius = w->radius;
        w->radius = 0;
    }
    if (w->border > 0) {
        leaf->border = w->border;
        leaf->border_c = w->border_c;
        w->border = 0;
    }
    if (w->font_family[0]) {
        str_copy(leaf->font_family, sizeof(leaf->font_family), w->font_family);
        w->font_family[0] = 0;
    }
    if (w->font_size > 0) {
        leaf->font_size = w->font_size;
        w->font_size = 0;
    }
    if (w->font_weight > 0) {
        leaf->font_weight = w->font_weight;
        w->font_weight = 0;
    }
    if (w->text_align) {
        leaf->text_align = w->text_align;
        w->text_align = 0;
    }
    if (w->overflow) {
        leaf->overflow = w->overflow;
        w->overflow = 0;
    }
}

static int expand_component(BuildCtx *ctx, const char *name, const XmlStart *overrides)
{
    if (!ctx || !ctx->res || !name)
        return -1;
    const char *path = NULL;
    for (int i = 0; i < ctx->res->component_count; i++) {
        if (str_eq(ctx->res->components[i].name, name)) {
            path = ctx->res->components[i].path;
            break;
        }
    }
    if (!path)
        return -1;

    int idx = push_widget(ctx, W_VBOX);
    if (idx < 0)
        return -1;
    Widget *w = &ctx->s->widgets[idx];
    w->type = W_VBOX;
    w->flex_dir = FLEX_COL;
    w->wu = UNIT_FILL;
    w->hu = UNIT_AUTO;
    w->grow = 0;
    str_copy(w->component, sizeof(w->component), name);

    int saved_sp = ctx->sp;
    CompCtx cc = {ctx, idx, 0};
    XmlHandlers h;
    memset(&h, 0, sizeof(h));
    h.on_start = comp_start;
    h.on_end = comp_end;
    h.on_text = on_text;
    h.user = &cc;
    ctx->in_component++;
    xml_parse_file(path, &h);
    ctx->in_component--;
    ctx->sp = saved_sp;
    if (overrides) {
        xml_ui_apply_attrs(w, overrides, ctx->res);
        Widget *leaf = NULL;
        for (int i = 0; i < w->child_count; i++) {
            Widget *child = &ctx->s->widgets[w->children[i]];
            if (child->type == W_BUTTON || child->type == W_LABEL || child->type == W_INPUT) {
                leaf = child;
                break;
            }
        }
        if (leaf) {
            transfer_to_leaf(w, leaf, name);
        } else if (w->id[0] && w->child_count > 0 && str_eq(name, "keypad")) {
            Widget *first = &ctx->s->widgets[w->children[0]];
            str_copy(first->id, sizeof(first->id), w->id);
            w->id[0] = 0;
        }
        str_copy(w->component, sizeof(w->component), name);
    }
    return idx;
}

int xml_ui_load_screen(Screen *s, const char *path, XmlUiResources *res)
{
    if (!s || !path)
        return -1;
    memset(s, 0, sizeof(*s));
    s->root = -1;
    s->focus = -1;
    str_copy(s->path, sizeof(s->path), path);
    str_copy(s->soft_left, sizeof(s->soft_left), "Menu");
    str_copy(s->soft_center, sizeof(s->soft_center), "OK");
    str_copy(s->soft_right, sizeof(s->soft_right), "Back");

    if (res)
        xml_ui_res_load_package(res, path);

    BuildCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.s = s;
    ctx.res = res;

    XmlHandlers h;
    memset(&h, 0, sizeof(h));
    h.on_start = on_start;
    h.on_end = on_end;
    h.on_text = on_text;
    h.user = &ctx;

    if (xml_parse_file(path, &h) != 0)
        return -1;

    if (s->root < 0 && s->count > 0)
        s->root = 0;
    return 0;
}
