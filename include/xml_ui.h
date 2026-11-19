#ifndef VILAOS_XML_UI_H
#define VILAOS_XML_UI_H

#include "types.h"
#include "xml.h"

#define XML_UI_MAX_COLORS 64
#define XML_UI_MAX_DIMS 64
#define XML_UI_MAX_COMPONENTS 96
#define XML_UI_MAX_STYLES 64
#define XML_UI_STYLE_PROPS 12

typedef struct {
    char name[MAX_ID];
    uint16_t color;
} XmlUiColor;

typedef struct {
    char name[MAX_ID];
    int val;
    int unit;
} XmlUiDim;

typedef struct {
    char name[MAX_ID];
    char path[MAX_PATH];
} XmlUiComponent;

typedef struct {
    char name[MAX_ID];
    char value[MAX_ATTR];
} XmlUiStyleProp;

typedef struct {
    char name[MAX_ID];
    XmlUiStyleProp props[XML_UI_STYLE_PROPS];
    int prop_count;
} XmlUiStyle;

typedef struct {
    XmlUiColor colors[XML_UI_MAX_COLORS];
    int color_count;
    XmlUiDim dims[XML_UI_MAX_DIMS];
    int dim_count;
    XmlUiComponent components[XML_UI_MAX_COMPONENTS];
    int component_count;
    XmlUiStyle styles[XML_UI_MAX_STYLES];
    int style_count;
    char app_root[MAX_PATH];
    char os_root[MAX_PATH];
} XmlUiResources;

void xml_ui_res_init(XmlUiResources *r);
int xml_ui_res_load_package(XmlUiResources *r, const char *layout_path);
int xml_ui_res_resolve_color(const XmlUiResources *r, const char *spec, uint16_t *out, uint16_t def);
int xml_ui_res_resolve_dim(const XmlUiResources *r, const char *spec, int *val, int *unit);

int xml_ui_style_load_file(XmlUiResources *r, const char *path);
int xml_ui_style_load_dir(XmlUiResources *r, const char *dir);
const XmlUiStyle *xml_ui_style_find(const XmlUiResources *r, const char *name);
void xml_ui_style_apply_class(Widget *w, const char *classes, const XmlUiResources *res);
void xml_ui_style_apply_variant(Widget *w, const char *variant);
void xml_ui_style_apply_prop(Widget *w, const char *name, const char *value, const XmlUiResources *res);
void xml_ui_style_apply_props(Widget *w, const char *const *names, const char *const *values, int n,
                              const XmlUiResources *res);

void xml_ui_parse_spacing(const char *spec, int16_t *t, int16_t *r, int16_t *b, int16_t *l);
void xml_ui_apply_attrs(Widget *w, const XmlStart *el, const XmlUiResources *res);
WidgetType xml_ui_tag_type(const char *tag);

int xml_ui_load_screen(Screen *s, const char *path, XmlUiResources *res);
void xml_ui_layout(Screen *s, Gfx *g);
void xml_ui_clip_children(Screen *s, Widget *container);

int xml_ui_resolve_dim(int src, unsigned unit, int parent, int screen, int ref);

#endif
