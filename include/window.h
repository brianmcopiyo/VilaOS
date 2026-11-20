#ifndef VILAOS_WINDOW_H
#define VILAOS_WINDOW_H

#include "types.h"
#include "screen.h"
#include "route.h"
#include "pkg.h"
#include "fb.h"
#include "input.h"

#define WM_MAX_WINDOWS 8
#define WM_TITLE_H 28
#define WM_EDGE 5
#define WM_BTN_W 32
#define WM_MIN_W 180
#define WM_MIN_H 140
#define WM_TASK_SLOTS 6

#define WM_ST_NORMAL 0
#define WM_ST_MINIMIZED 1
#define WM_ST_MAXIMIZED 2

#define WM_HIT_NONE 0
#define WM_HIT_CLIENT 1
#define WM_HIT_MOVE 2
#define WM_HIT_MIN 3
#define WM_HIT_MAX 4
#define WM_HIT_CLOSE 5
#define WM_HIT_RESIZE_E 6
#define WM_HIT_RESIZE_S 7
#define WM_HIT_RESIZE_SE 8
#define WM_HIT_RESIZE_W 9
#define WM_HIT_RESIZE_N 10
#define WM_HIT_RESIZE_NW 11
#define WM_HIT_RESIZE_NE 12
#define WM_HIT_RESIZE_SW 13

typedef struct {
    int id;
    int alive;
    int z;
    int state;
    int focused;
    int protected_close;
    int x, y, w, h;
    int rx, ry, rw, rh;
    char app_id[MAX_ID];
    char title[MAX_TEXT];
    char script[MAX_PATH];
    PkgApp app;
    Screen screen;
    RouteStack stack;
} OsWindow;

typedef struct {
    OsWindow wins[WM_MAX_WINDOWS];
    int count;
    int focus_id;
    int z_top;
    int next_id;
    int drag_mode;
    int drag_id;
    int drag_ox;
    int drag_oy;
    int drag_bx;
    int drag_by;
    int drag_bw;
    int drag_bh;
    int work_x;
    int work_y;
    int work_w;
    int work_h;
    Screen *desktop;
    Gfx *gfx;
} WindowManager;

extern WindowManager g_wm;

void wm_init(WindowManager *wm, Screen *desktop, Gfx *g);
void wm_set_work_area(WindowManager *wm, int x, int y, int w, int h);
OsWindow *wm_find(WindowManager *wm, int id);
OsWindow *wm_find_app(WindowManager *wm, const char *app_id);
OsWindow *wm_focused(WindowManager *wm);
Screen *wm_active_screen(WindowManager *wm);
int wm_open(WindowManager *wm, const PkgApp *app, const char *script, const char *title);
int wm_focus(WindowManager *wm, int id);
int wm_minimize(WindowManager *wm, int id);
int wm_toggle_max(WindowManager *wm, int id);
int wm_close(WindowManager *wm, int id);
int wm_close_app(WindowManager *wm, const char *app_id);
int wm_is_protected_app(const char *app_id);
void wm_layout_window(WindowManager *wm, OsWindow *win);
void wm_layout_all(WindowManager *wm);
void wm_render(WindowManager *wm, Gfx *g);
void wm_sync_taskbar(WindowManager *wm, Screen *desktop);
int wm_hit_test(WindowManager *wm, int x, int y, int *out_id);
int wm_pointer(WindowManager *wm, InputMux *m, InputEvent *ev, Screen **out_screen, int *out_lx, int *out_ly);
int wm_handle_action(WindowManager *wm, const char *action);

#endif
