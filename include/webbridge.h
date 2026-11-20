#ifndef VILAOS_WEBBRIDGE_H
#define VILAOS_WEBBRIDGE_H

#include "types.h"
#include "window.h"
#include "pkg.h"
#include "fb.h"

typedef struct WebBridge WebBridge;

int webbridge_start(WebBridge **out, Gfx *gfx, WindowManager *wm, int port);
void webbridge_stop(WebBridge *wb);
int webbridge_poll(WebBridge *wb);
int webbridge_running(const WebBridge *wb);
int webbridge_port(const WebBridge *wb);
void webbridge_notify_widgets(WebBridge *wb);
void webbridge_set_root(WebBridge *wb, const char *root);

#endif
