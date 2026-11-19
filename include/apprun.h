#ifndef VILAOS_APPRUN_H
#define VILAOS_APPRUN_H

#include "pkg.h"
#include "vsl.h"
#include "screen.h"
#include "route.h"

typedef struct {
    PkgRegistry reg;
    PkgApp current;
    VslRuntime vsl;
    RouteStack stack;
    int in_app;
    char shell_path[MAX_PATH];
} AppHost;

int apphost_init(AppHost *h);
int apphost_refresh_launcher(AppHost *h, Screen *s);
int apphost_open_app(AppHost *h, Screen *s, Gfx *g, const char *id);
int apphost_open_screen(AppHost *h, Screen *s, Gfx *g, const char *rel_or_abs);
int apphost_handle_action(AppHost *h, Screen *s, Gfx *g, const char *action, char *out_nav, size_t out_sz);
int apphost_fire_event(AppHost *h, const char *event, const char *target);
int apphost_on_tick(AppHost *h);
int apphost_route_push(AppHost *h, Screen *s, Gfx *g, const char *layout);
int apphost_route_replace(AppHost *h, Screen *s, Gfx *g, const char *layout);
int apphost_route_pop(AppHost *h, Screen *s, Gfx *g, char *out_nav, size_t out_sz);
int apphost_route_home(AppHost *h, Screen *s, Gfx *g, char *out_nav, size_t out_sz);
void apphost_update_softkeys(AppHost *h, Screen *s);

#endif
