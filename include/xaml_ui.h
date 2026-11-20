#ifndef VILAOS_XAML_UI_H
#define VILAOS_XAML_UI_H

#include "xml_ui.h"

void xaml_normalize_tag(char *tag);
void xaml_normalize_attr_name(const char *in, char *out, size_t n);
const char *xaml_attr(const XmlStart *el, const char *name);
void xaml_apply_attrs(Widget *w, const XmlStart *el, const XmlUiResources *res);
WidgetType xaml_tag_type(const char *tag);
int xaml_ui_load_screen(Screen *s, const char *path, XmlUiResources *res);

#endif
