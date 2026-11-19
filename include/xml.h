#ifndef PHONE_XML_H
#define PHONE_XML_H

#include "types.h"

typedef struct {
    char name[MAX_ID];
    char value[MAX_ATTR];
} XmlAttr;

typedef struct {
    char tag[MAX_ID];
    XmlAttr attrs[16];
    int attr_count;
    int self_close;
} XmlStart;

typedef void (*XmlOnStart)(const XmlStart *el, void *user);
typedef void (*XmlOnEnd)(const char *tag, void *user);
typedef void (*XmlOnText)(const char *text, void *user);

typedef struct {
    XmlOnStart on_start;
    XmlOnEnd on_end;
    XmlOnText on_text;
    void *user;
} XmlHandlers;

int xml_parse_file(const char *path, const XmlHandlers *h);
const char *xml_attr(const XmlStart *el, const char *name);

#endif
