#ifndef PHONE_INPUT_H
#define PHONE_INPUT_H

#include "types.h"

#include <pthread.h>

#define INPUT_MAX_TOUCH_SLOTS 8
#define INPUT_Q_MAX 64

typedef enum {
    IE_NONE = 0,
    IE_NAV_UP,
    IE_NAV_DOWN,
    IE_NAV_LEFT,
    IE_NAV_RIGHT,
    IE_OK,
    IE_BACK,
    IE_HOME,
    IE_SOFT_LEFT,
    IE_SOFT_RIGHT,
    IE_CHAR,
    IE_BACKSPACE,
    IE_CLICK,
    IE_TOUCH_DOWN,
    IE_TOUCH_MOVE,
    IE_TOUCH_UP,
    IE_POWER,
    IE_QUIT
} InputEventKind;

typedef struct {
    InputEventKind kind;
    char ch;
    int x, y;
    int id;
    int slot;
} InputEvent;

typedef struct {
    int abs_x_min, abs_x_max;
    int abs_y_min, abs_y_max;
    int has_abs;
    int has_mt;
    int is_touch;
    int abs_pointer;
    int ignore_rel;
} InputDevCaps;

typedef struct {
    int active;
    int id;
    int x, y;
} TouchSlot;

typedef struct {
    int epfd;
    int inotify_fd;
    int tty_fd;
    int fds[MAX_INPUT_DEVS];
    int grabbed[MAX_INPUT_DEVS];
    InputDevCaps caps[MAX_INPUT_DEVS];
    int fd_count;
    InputState state;
    T9State t9;
    int dialer_mode;
    TouchSlot slots[INPUT_MAX_TOUCH_SLOTS];
    int cur_slot;
    int touch_down;
    int touch_x;
    int touch_y;
    int touch_id;
    int pend_x;
    int pend_y;
    int pend_x_raw;
    int pend_y_raw;
    int have_x;
    int have_y;
    int btn_touch;
    int btn_left;
    int mt_tracking;
    int gesture_active;
    int gesture_scroll;
    int gesture_start_x;
    int gesture_start_y;
    int gesture_last_y;
    int gesture_last_x;
    int gesture_target;
    uint64_t gesture_start_ms;
    pthread_t pump_thr;
    pthread_mutex_t pump_mu;
    pthread_cond_t pump_cv;
    int pump_run;
    int pump_started;
    InputEvent q[INPUT_Q_MAX];
    int q_head;
    int q_tail;
    int q_count;
    uint32_t pointer_gen;
    int coalesce_x;
    int coalesce_y;
    int coalesce_vis;
} InputMux;

int input_init(InputMux *m);
void input_shutdown(InputMux *m);
int input_poll(InputMux *m, InputEvent *out, int timeout_ms);
void input_set_dialer_mode(InputMux *m, int enabled);
void input_apply_char(Widget *w, char ch);
void input_backspace(Widget *w);
void input_mode_label(const InputMux *m, char *buf, size_t n);
int input_tap_slop(const InputMux *m);
uint32_t input_pointer_gen(const InputMux *m);

#endif
