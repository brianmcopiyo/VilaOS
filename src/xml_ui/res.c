#include "xml_ui.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <dirent.h>
#endif

void xml_ui_res_init(XmlUiResources *r)
{
    if (!r)
        return;
    memset(r, 0, sizeof(*r));
    str_copy(r->os_root, sizeof(r->os_root), "assets");
}

static void add_color(XmlUiResources *r, const char *name, uint16_t c)
{
    if (!r || !name || !name[0] || r->color_count >= XML_UI_MAX_COLORS)
        return;
    str_copy(r->colors[r->color_count].name, sizeof(r->colors[r->color_count].name), name);
    r->colors[r->color_count].color = c;
    r->color_count++;
}

static void add_dim(XmlUiResources *r, const char *name, int val, int unit)
{
    if (!r || !name || !name[0] || r->dim_count >= XML_UI_MAX_DIMS)
        return;
    str_copy(r->dims[r->dim_count].name, sizeof(r->dims[r->dim_count].name), name);
    r->dims[r->dim_count].val = val;
    r->dims[r->dim_count].unit = unit;
    r->dim_count++;
}

typedef struct {
    XmlUiResources *r;
} ValCtx;

static void values_start(const XmlStart *el, void *user)
{
    ValCtx *ctx = (ValCtx *)user;
    if (str_eq(el->tag, "color")) {
        const char *name = xml_attr(el, "name");
        const char *val = xml_attr(el, "value");
        uint16_t c;
        if (name && val && parse_hex_color(val, &c) == 0)
            add_color(ctx->r, name, c);
    } else if (str_eq(el->tag, "dimen") || str_eq(el->tag, "dim")) {
        const char *name = xml_attr(el, "name");
        const char *val = xml_attr(el, "value");
        int v = 0, u = UNIT_PX;
        if (name && val && parse_dim(val, &v, &u) == 0)
            add_dim(ctx->r, name, v, u);
    }
}

static int load_values_file(XmlUiResources *r, const char *path)
{
    ValCtx ctx = {r};
    XmlHandlers h;
    memset(&h, 0, sizeof(h));
    h.on_start = values_start;
    h.user = &ctx;
    return xml_parse_file(path, &h);
}

static int find_component(const XmlUiResources *r, const char *name)
{
    for (int i = 0; i < r->component_count; i++) {
        if (str_eq(r->components[i].name, name))
            return i;
    }
    return -1;
}

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

static void register_component(XmlUiResources *r, const char *name, const char *path)
{
    if (!r || !name || !path || r->component_count >= XML_UI_MAX_COMPONENTS)
        return;
    if (find_component(r, name) >= 0)
        return;
    str_copy(r->components[r->component_count].name, sizeof(r->components[r->component_count].name), name);
    str_copy(r->components[r->component_count].path, sizeof(r->components[r->component_count].path), path);
    r->component_count++;
}

static void scan_components(XmlUiResources *r, const char *dir)
{
#ifndef _WIN32
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        size_t n = strlen(de->d_name);
        if (n < 5 || !str_eq(de->d_name + n - 4, ".xml"))
            continue;
        char name[MAX_ID];
        size_t len = n - 4;
        if (len >= sizeof(name))
            len = sizeof(name) - 1;
        memcpy(name, de->d_name, len);
        name[len] = 0;
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
        register_component(r, name, path);
    }
    closedir(d);
#else
    (void)r;
    (void)dir;
#endif
}

static const char *const k_ui_components[] = {
    "badge", "card", "input", "keypad", "list", "footer", "tile", "row", "header", "button", "text", "icon"
};

static const char *const k_base_components[] = {
    "button", "divider", "input", "tile", "row", "badge", "header", "text", "icon"
};

static void register_static_dir(XmlUiResources *r, const char *dir, const char *const *names, size_t count)
{
    if (!dir || !dir[0])
        return;
    for (size_t i = 0; i < count; i++) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s.xml", dir, names[i]);
        if (file_exists(path))
            register_component(r, names[i], path);
    }
}

static void register_static_components(XmlUiResources *r)
{
    const char *os = r->os_root[0] ? r->os_root : "assets";
    char dir[MAX_PATH];
    size_t nu = sizeof(k_ui_components) / sizeof(k_ui_components[0]);
    size_t nb = sizeof(k_base_components) / sizeof(k_base_components[0]);

    if (r->app_root[0]) {
        snprintf(dir, sizeof(dir), "%s/res/components", r->app_root);
        register_static_dir(r, dir, k_ui_components, nu);
        register_static_dir(r, dir, k_base_components, nb);
    }
    register_static_dir(r, "sdk/ui", k_ui_components, nu);
    register_static_dir(r, "assets/ui", k_ui_components, nu);
    snprintf(dir, sizeof(dir), "%s/ui", os);
    register_static_dir(r, dir, k_ui_components, nu);
    register_static_dir(r, "sdk/components", k_base_components, nb);
    register_static_dir(r, "assets/components", k_base_components, nb);
    snprintf(dir, sizeof(dir), "%s/components", os);
    register_static_dir(r, dir, k_base_components, nb);
}

static void register_alias(XmlUiResources *r, const char *alias, const char *target)
{
    if (find_component(r, alias) >= 0)
        return;
    int i = find_component(r, target);
    if (i < 0)
        return;
    char path[MAX_PATH];
    str_copy(path, sizeof(path), r->components[i].path);
    register_component(r, alias, path);
}

static void load_styles(XmlUiResources *r)
{
    char dir[MAX_PATH];
    if (r->os_root[0] && !str_eq(r->os_root, "assets")) {
        snprintf(dir, sizeof(dir), "%s/styles", r->os_root);
        xml_ui_style_load_dir(r, dir);
    }
    xml_ui_style_load_dir(r, "assets/styles");
    xml_ui_style_load_dir(r, "res/styles");
    xml_ui_style_load_dir(r, "sdk/styles");
    if (r->app_root[0]) {
        snprintf(dir, sizeof(dir), "%s/res/styles", r->app_root);
        xml_ui_style_load_dir(r, dir);
    }
}

static void derive_roots(XmlUiResources *r, const char *layout_path)
{
    if (!r || !layout_path)
        return;
    char tmp[MAX_PATH];
    str_copy(tmp, sizeof(tmp), layout_path);
    char *p = strstr(tmp, "/res/layout/");
    if (!p)
        p = strstr(tmp, "\\res\\layout\\");
    if (p) {
        *p = 0;
        str_copy(r->app_root, sizeof(r->app_root), tmp);
        return;
    }
    p = strstr(tmp, "/assets/screens/");
    if (!p)
        p = strstr(tmp, "\\assets\\screens\\");
    if (p) {
        *p = 0;
        str_copy(r->os_root, sizeof(r->os_root), tmp[0] ? tmp : "assets");
        if (!r->os_root[0])
            str_copy(r->os_root, sizeof(r->os_root), "assets");
    }
}

int xml_ui_res_load_package(XmlUiResources *r, const char *layout_path)
{
    if (!r)
        return -1;
    xml_ui_res_init(r);
    derive_roots(r, layout_path);

    add_color(r, "bg", COL_BG);
    add_color(r, "panel", COL_PANEL);
    add_color(r, "border", COL_BORDER);
    add_color(r, "text", COL_TEXT);
    add_color(r, "dim", COL_DIM);
    add_color(r, "accent", COL_ACCENT);
    add_color(r, "focus", COL_FOCUS);
    add_color(r, "sunset", COL_ACCENT);
    add_color(r, "beige", COL_BG);
    add_color(r, "sand", COL_PANEL);
    add_color(r, "surface", COL_PANEL);
    add_color(r, "secondary", COL_DIM);
    add_color(r, "on_primary", COL_ON_ACCENT);
    add_color(r, "positive", COL_OK);
    add_color(r, "destructive", COL_DANGER);

    char path[MAX_PATH];
    if (r->app_root[0]) {
        snprintf(path, sizeof(path), "%s/res/values/colors.xml", r->app_root);
        load_values_file(r, path);
        snprintf(path, sizeof(path), "%s/res/values/dimens.xml", r->app_root);
        load_values_file(r, path);
        snprintf(path, sizeof(path), "%s/res/components", r->app_root);
        scan_components(r, path);
    }
    snprintf(path, sizeof(path), "%s/values/colors.xml", r->os_root[0] ? r->os_root : "assets");
    load_values_file(r, path);
    snprintf(path, sizeof(path), "%s/values/dimens.xml", r->os_root[0] ? r->os_root : "assets");
    load_values_file(r, path);
    register_static_components(r);
    scan_components(r, "sdk/ui");
    scan_components(r, "assets/ui");
    snprintf(path, sizeof(path), "%s/ui", r->os_root[0] ? r->os_root : "assets");
    scan_components(r, path);
    snprintf(path, sizeof(path), "%s/components", r->os_root[0] ? r->os_root : "assets");
    scan_components(r, path);
    register_alias(r, "input_field", "input");
    register_alias(r, "launcher_tile", "tile");
    register_alias(r, "list_row", "row");
    register_alias(r, "message_row", "row");
    register_alias(r, "file_row", "row");
    register_alias(r, "action_button", "button");
    register_alias(r, "secondary_button", "button");
    register_alias(r, "notification_pill", "badge");
    register_alias(r, "status_badge", "badge");
    register_alias(r, "section_header", "header");
    register_alias(r, "list_view", "list");
    register_alias(r, "softkey_bar", "footer");
    load_styles(r);
    return 0;
}

int xml_ui_res_resolve_color(const XmlUiResources *r, const char *spec, uint16_t *out, uint16_t def)
{
    if (!out)
        return -1;
    *out = def;
    if (!spec || !spec[0])
        return 0;
    if (spec[0] == '@') {
        const char *name = spec;
        if (str_starts(spec, "@color/"))
            name = spec + 7;
        else
            name = spec + 1;
        if (r) {
            for (int i = 0; i < r->color_count; i++) {
                if (str_eq(r->colors[i].name, name)) {
                    *out = r->colors[i].color;
                    return 0;
                }
            }
        }
        return -1;
    }
    return parse_hex_color(spec, out);
}

int xml_ui_res_resolve_dim(const XmlUiResources *r, const char *spec, int *val, int *unit)
{
    if (!val || !unit)
        return -1;
    *val = 0;
    *unit = UNIT_AUTO;
    if (!spec || !spec[0])
        return 0;
    if (spec[0] == '@') {
        const char *name = spec;
        if (str_starts(spec, "@dimen/"))
            name = spec + 7;
        else
            name = spec + 1;
        if (r) {
            for (int i = 0; i < r->dim_count; i++) {
                if (str_eq(r->dims[i].name, name)) {
                    *val = r->dims[i].val;
                    *unit = r->dims[i].unit;
                    return 0;
                }
            }
        }
        return -1;
    }
    return parse_dim(spec, val, unit);
}
