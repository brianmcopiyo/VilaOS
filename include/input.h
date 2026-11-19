#ifndef PHONE_INPUT_H
#define PHONE_INPUT_H

#include "types.h"

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
    IE_QUIT
} InputEventKind;

typedef struct {
    InputEventKind kind;
    char ch;
    int x, y;
} InputEvent;

typedef struct {
    int epfd;
    int inotify_fd;
    int tty_fd;
    int fds[MAX_INPUT_DEVS];
    int grabbed[MAX_INPUT_DEVS];
    int fd_count;
    InputState state;
    T9State t9;
    int dialer_mode;
} InputMux;

int input_init(InputMux *m);
void input_shutdown(InputMux *m);
int input_poll(InputMux *m, InputEvent *out, int timeout_ms);
void input_set_dialer_mode(InputMux *m, int enabled);
void input_apply_char(Widget *w, char ch);
void input_backspace(Widget *w);
void input_mode_label(const InputMux *m, char *buf, size_t n);

#endif
