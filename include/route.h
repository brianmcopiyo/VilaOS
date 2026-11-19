#ifndef VILAOS_ROUTE_H
#define VILAOS_ROUTE_H

#include "types.h"
#include "pkg.h"

#define MAX_ROUTE_DEPTH 8
#define MAX_ROUTE_PARAMS 64

typedef struct {
    char app_id[PKG_ID_LEN];
    char layout[MAX_PATH];
    int focus;
    char params[MAX_ROUTE_PARAMS];
} RouteFrame;

typedef struct {
    RouteFrame frames[MAX_ROUTE_DEPTH];
    int depth;
} RouteStack;

void route_reset(RouteStack *st);
int route_depth(const RouteStack *st);
const RouteFrame *route_top(const RouteStack *st);
int route_push(RouteStack *st, const char *app_id, const char *layout, int focus, const char *params);
int route_replace(RouteStack *st, const char *layout, int focus, const char *params);
int route_pop(RouteStack *st, RouteFrame *out);
int route_save_focus(RouteStack *st, int focus);
int route_is_root(const RouteStack *st);

#endif
