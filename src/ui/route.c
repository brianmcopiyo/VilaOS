#include "route.h"
#include "util.h"

#include <string.h>

void route_reset(RouteStack *st)
{
    if (!st)
        return;
    memset(st, 0, sizeof(*st));
}

int route_depth(const RouteStack *st)
{
    return st ? st->depth : 0;
}

const RouteFrame *route_top(const RouteStack *st)
{
    if (!st || st->depth <= 0)
        return NULL;
    return &st->frames[st->depth - 1];
}

int route_push(RouteStack *st, const char *app_id, const char *layout, int focus, const char *params)
{
    if (!st || !app_id || !layout || st->depth >= MAX_ROUTE_DEPTH)
        return -1;
    RouteFrame *f = &st->frames[st->depth++];
    memset(f, 0, sizeof(*f));
    str_copy(f->app_id, sizeof(f->app_id), app_id);
    str_copy(f->layout, sizeof(f->layout), layout);
    f->focus = focus;
    if (params)
        str_copy(f->params, sizeof(f->params), params);
    return 0;
}

int route_replace(RouteStack *st, const char *layout, int focus, const char *params)
{
    if (!st || !layout || st->depth <= 0)
        return -1;
    RouteFrame *f = &st->frames[st->depth - 1];
    str_copy(f->layout, sizeof(f->layout), layout);
    f->focus = focus;
    if (params)
        str_copy(f->params, sizeof(f->params), params);
    else
        f->params[0] = 0;
    return 0;
}

int route_pop(RouteStack *st, RouteFrame *out)
{
    if (!st || st->depth <= 0)
        return -1;
    st->depth--;
    if (out)
        *out = st->frames[st->depth];
    return 0;
}

int route_save_focus(RouteStack *st, int focus)
{
    if (!st || st->depth <= 0)
        return -1;
    st->frames[st->depth - 1].focus = focus;
    return 0;
}

int route_is_root(const RouteStack *st)
{
    return !st || st->depth <= 1;
}
