#include "xml.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

const char *xml_attr(const XmlStart *el, const char *name)
{
    if (!el || !name)
        return NULL;
    for (int i = 0; i < el->attr_count; i++) {
        if (str_eq(el->attrs[i].name, name))
            return el->attrs[i].value;
    }
    return NULL;
}

static const char *skip_ws(const char *p)
{
    while (*p && isspace((unsigned char)*p))
        p++;
    return p;
}

static const char *parse_name(const char *p, char *out, size_t out_sz)
{
    size_t i = 0;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '-' || *p == ':')) {
        if (i + 1 < out_sz)
            out[i++] = *p;
        p++;
    }
    out[i] = 0;
    return p;
}

static const char *parse_value(const char *p, char *out, size_t out_sz)
{
    char quote = *p;
    if (quote != '"' && quote != '\'') {
        out[0] = 0;
        return p;
    }
    p++;
    size_t i = 0;
    while (*p && *p != quote) {
        if (i + 1 < out_sz)
            out[i++] = *p;
        p++;
    }
    out[i] = 0;
    if (*p == quote)
        p++;
    return p;
}

static const char *parse_attrs(const char *p, XmlStart *el)
{
    el->attr_count = 0;
    for (;;) {
        p = skip_ws(p);
        if (!*p || *p == '>' || *p == '/')
            return p;
        if (el->attr_count >= 16)
            return p;
        XmlAttr *a = &el->attrs[el->attr_count];
        p = parse_name(p, a->name, sizeof(a->name));
        p = skip_ws(p);
        if (*p != '=') {
            a->value[0] = 0;
            el->attr_count++;
            continue;
        }
        p++;
        p = skip_ws(p);
        p = parse_value(p, a->value, sizeof(a->value));
        el->attr_count++;
    }
}

static void emit_text(const char *start, const char *end, const XmlHandlers *h)
{
    if (!h->on_text || start >= end)
        return;
    while (start < end && isspace((unsigned char)*start))
        start++;
    while (end > start && isspace((unsigned char)*(end - 1)))
        end--;
    if (start >= end)
        return;
    char buf[MAX_TEXT];
    size_t n = (size_t)(end - start);
    if (n >= sizeof(buf))
        n = sizeof(buf) - 1;
    memcpy(buf, start, n);
    buf[n] = 0;
    h->on_text(buf, h->user);
}

int xml_parse_file(const char *path, const XmlHandlers *h)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;

    static char data[65536];
    size_t n = fread(data, 1, sizeof(data) - 1, f);
    fclose(f);
    data[n] = 0;

    const char *p = data;
    while (*p) {
        const char *lt = strchr(p, '<');
        if (!lt) {
            emit_text(p, p + strlen(p), h);
            break;
        }
        emit_text(p, lt, h);
        if (lt[1] == '/' ) {
            char tag[MAX_ID];
            const char *q = parse_name(lt + 2, tag, sizeof(tag));
            q = skip_ws(q);
            if (*q == '>')
                q++;
            if (h->on_end)
                h->on_end(tag, h->user);
            p = q;
            continue;
        }
        if (lt[1] == '!' || lt[1] == '?') {
            const char *gt = strchr(lt + 2, '>');
            p = gt ? gt + 1 : lt + 1;
            continue;
        }
        XmlStart el;
        memset(&el, 0, sizeof(el));
        const char *q = parse_name(lt + 1, el.tag, sizeof(el.tag));
        q = parse_attrs(q, &el);
        q = skip_ws(q);
        el.self_close = 0;
        if (q[0] == '/' && q[1] == '>') {
            el.self_close = 1;
            q += 2;
        } else if (*q == '>') {
            q++;
        }
        if (h->on_start)
            h->on_start(&el, h->user);
        p = q;
    }
    return 0;
}
