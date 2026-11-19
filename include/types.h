#ifndef PHONE_TYPES_H
#define PHONE_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define REF_W 240
#define REF_H 320
#define MAX_FB_W 1920
#define MAX_FB_H 1200

#define MAX_WIDGETS 96
#define MAX_TEXT 128
#define MAX_ID 32
#define MAX_ATTR 64
#define MAX_CHILDREN 32
#define MAX_INPUT_DEVS 16
#define MAX_PATH 256
#define MAX_SOFTKEY 16

#define UNIT_PX 0
#define UNIT_PCT 1
#define UNIT_AUTO 2

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))

#define COL_BG       RGB565(12, 18, 28)
#define COL_PANEL    RGB565(24, 36, 52)
#define COL_BORDER   RGB565(48, 72, 96)
#define COL_TEXT     RGB565(220, 228, 236)
#define COL_DIM      RGB565(120, 136, 152)
#define COL_ACCENT   RGB565(40, 160, 220)
#define COL_FOCUS    RGB565(20, 90, 140)
#define COL_SOFTKEY  RGB565(18, 28, 40)
#define COL_WARN     RGB565(220, 160, 40)
#define COL_OK       RGB565(40, 180, 90)

typedef enum {
    W_SCREEN = 0,
    W_VBOX,
    W_HBOX,
    W_LABEL,
    W_BUTTON,
    W_INPUT,
    W_LIST,
    W_STATUSBAR,
    W_SOFTKEYS
} WidgetType;

typedef enum {
    ACT_NONE = 0,
    ACT_NAV,
    ACT_CALL,
    ACT_CLEAR,
    ACT_BACK,
    ACT_COMPOSE,
    ACT_SEND,
    ACT_SELECT
} ActionKind;

typedef struct {
    int x, y, w, h;
} Rect;

typedef struct Widget Widget;

struct Widget {
    WidgetType type;
    char id[MAX_ID];
    char text[MAX_TEXT];
    char action[MAX_PATH];
    int x, y, w, h;
    int sx, sy, sw, sh;
    unsigned xu : 2;
    unsigned yu : 2;
    unsigned wu : 2;
    unsigned hu : 2;
    uint16_t bg, fg, focus_bg;
    int max_len;
    int focusable;
    int focused;
    int visible;
    int parent;
    int child_count;
    int children[MAX_CHILDREN];
    int list_sel;
    int scroll;
};

typedef struct {
    int x0, y0, x1, y1;
    int valid;
} DirtyRect;

typedef struct {
    uint16_t *back;
    size_t back_pixels;
    void *fb_map;
    int fb_fd;
    int w, h;
    int fb_w, fb_h, fb_bpp, fb_stride;
    int is_xrgb;
    int font_scale;
    int status_h, softkey_h;
    int pad;
    int item_h;
    size_t fb_map_len;
    DirtyRect dirty;
    int desktop;
    int resized;
} Gfx;

typedef struct {
    Widget widgets[MAX_WIDGETS];
    int count;
    int root;
    int focus;
    char path[MAX_PATH];
    char soft_left[MAX_SOFTKEY];
    char soft_center[MAX_SOFTKEY];
    char soft_right[MAX_SOFTKEY];
    char soft_left_act[MAX_PATH];
    char soft_center_act[MAX_PATH];
    char soft_right_act[MAX_PATH];
} Screen;

typedef struct {
    int cursor_x, cursor_y;
    int cursor_visible;
    uint64_t last_pointer_ms;
    uint64_t last_key_ms;
    int shift;
    int caps;
    int bound_w, bound_h;
} InputState;

typedef struct {
    char buf[8];
    int len;
    int key;
    uint64_t last_ms;
    int upper;
} T9State;

#endif
