#include "input.h"
#include "t9.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if !defined(PHONE_DESKTOP) && !defined(PHONE_VFB)
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <linux/vt.h>
#else
#ifndef KEY_UP
#define KEY_UP 103
#define KEY_DOWN 108
#define KEY_LEFT 105
#define KEY_RIGHT 106
#define KEY_ENTER 28
#define KEY_KPENTER 96
#define KEY_ESC 1
#define KEY_BACKSPACE 14
#define KEY_F1 59
#define KEY_F2 60
#define KEY_F3 61
#define KEY_LEFTCTRL 29
#define KEY_RIGHTCTRL 97
#define KEY_LEFTSHIFT 42
#define KEY_RIGHTSHIFT 54
#define KEY_CAPSLOCK 58
#define KEY_HOME 102
#define KEY_LEFTMETA 125
#define KEY_RIGHTMETA 126
#define KEY_BACK 158
#define KEY_SPACE 57
#define KEY_1 2
#define KEY_2 3
#define KEY_3 4
#define KEY_4 5
#define KEY_5 6
#define KEY_6 7
#define KEY_7 8
#define KEY_8 9
#define KEY_9 10
#define KEY_0 11
#define KEY_A 30
#define KEY_B 48
#define KEY_C 46
#define KEY_D 32
#define KEY_E 18
#define KEY_F 33
#define KEY_G 34
#define KEY_H 35
#define KEY_I 23
#define KEY_J 36
#define KEY_K 37
#define KEY_L 38
#define KEY_M 50
#define KEY_N 49
#define KEY_O 24
#define KEY_P 25
#define KEY_Q 16
#define KEY_R 19
#define KEY_S 31
#define KEY_T 20
#define KEY_U 22
#define KEY_V 47
#define KEY_W 17
#define KEY_X 45
#define KEY_Y 21
#define KEY_Z 44
#define KEY_KP0 82
#define KEY_KP1 79
#define KEY_KP2 80
#define KEY_KP3 81
#define KEY_KP4 75
#define KEY_KP5 76
#define KEY_KP6 77
#define KEY_KP7 71
#define KEY_KP8 72
#define KEY_KP9 73
#define KEY_KPASTERISK 55
#define KEY_KPSLASH 98
#define KEY_MINUS 12
#define KEY_EQUAL 13
#define KEY_LEFTBRACE 26
#define KEY_RIGHTBRACE 27
#define KEY_SEMICOLON 39
#define KEY_APOSTROPHE 40
#define KEY_GRAVE 41
#define KEY_BACKSLASH 43
#define KEY_COMMA 51
#define KEY_DOT 52
#define KEY_SLASH 53
#define BTN_LEFT 0x110
#define EV_KEY 1
#define EV_REL 2
#define EV_ABS 3
#define REL_X 0
#define REL_Y 1
#define ABS_X 0x00
#define ABS_Y 0x01
#define ABS_MT_POSITION_X 0x35
#define ABS_MT_POSITION_Y 0x36
#endif
#endif

#ifdef PHONE_DESKTOP
#include <SDL2/SDL.h>
#endif

#ifdef PHONE_VFB
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
static struct termios g_old_term;
static int g_term_set;
#endif
#endif

void input_apply_char(Widget *w, char ch)
{
    if (!w || w->type != W_INPUT || !ch)
        return;
    size_t n = strlen(w->text);
    if ((int)n >= w->max_len || n + 1 >= sizeof(w->text))
        return;
    w->text[n] = ch;
    w->text[n + 1] = 0;
}

void input_backspace(Widget *w)
{
    if (!w || w->type != W_INPUT)
        return;
    size_t n = strlen(w->text);
    if (n > 0)
        w->text[n - 1] = 0;
}

#if !defined(PHONE_DESKTOP) && !defined(PHONE_VFB)
static int test_evbit(const unsigned long *bits, int ev)
{
    int long_bits = (int)(sizeof(unsigned long) * 8);
    return (bits[ev / long_bits] >> (ev % long_bits)) & 1UL;
}

static int device_interesting(int fd)
{
    unsigned long evbits[(EV_MAX / 8 / sizeof(long)) + 1];
    memset(evbits, 0, sizeof(evbits));
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits) < 0)
        return 1;
    return test_evbit(evbits, EV_KEY) || test_evbit(evbits, EV_REL) || test_evbit(evbits, EV_ABS);
}

static int add_device(InputMux *m, const char *path)
{
    if (m->fd_count >= MAX_INPUT_DEVS || m->epfd < 0)
        return -1;
    for (int i = 0; i < m->fd_count; i++) {
        char link[64];
        snprintf(link, sizeof(link), "/proc/self/fd/%d", m->fds[i]);
        char resolved[MAX_PATH];
        ssize_t n = readlink(link, resolved, sizeof(resolved) - 1);
        if (n > 0) {
            resolved[n] = 0;
            if (str_eq(resolved, path))
                return 0;
        }
    }
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return -1;
    if (!device_interesting(fd)) {
        close(fd);
        return -1;
    }
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    ev.data.fd = fd;
    if (epoll_ctl(m->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        close(fd);
        return -1;
    }
    if (ioctl(fd, EVIOCGRAB, 1) == 0)
        m->grabbed[m->fd_count] = 1;
    else
        m->grabbed[m->fd_count] = 0;
    m->fds[m->fd_count++] = fd;
    return 0;
}

static void remove_fd(InputMux *m, int fd)
{
    for (int i = 0; i < m->fd_count; i++) {
        if (m->fds[i] != fd)
            continue;
        if (m->grabbed[i])
            ioctl(fd, EVIOCGRAB, 0);
        if (m->epfd >= 0)
            epoll_ctl(m->epfd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        m->fds[i] = m->fds[m->fd_count - 1];
        m->grabbed[i] = m->grabbed[m->fd_count - 1];
        m->fd_count--;
        break;
    }
}

static void scan_devices(InputMux *m)
{
    DIR *d = opendir("/dev/input");
    if (!d)
        return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strncmp(de->d_name, "event", 5) != 0)
            continue;
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "/dev/input/%s", de->d_name);
        add_device(m, path);
    }
    closedir(d);
}

static void console_graphics(InputMux *m, int enable)
{
    if (m->tty_fd < 0)
        return;
    ioctl(m->tty_fd, KDSETMODE, enable ? KD_GRAPHICS : KD_TEXT);
}

static int open_console(InputMux *m)
{
    const char *paths[] = {"/dev/tty0", "/dev/tty", "/dev/console", NULL};
    for (int i = 0; paths[i]; i++) {
        int fd = open(paths[i], O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            m->tty_fd = fd;
            console_graphics(m, 1);
            return 0;
        }
    }
    m->tty_fd = -1;
    return -1;
}
#endif

int input_init(InputMux *m)
{
    memset(m, 0, sizeof(*m));
    m->epfd = -1;
    m->inotify_fd = -1;
    m->tty_fd = -1;
    m->state.bound_w = REF_W;
    m->state.bound_h = REF_H;
    m->state.cursor_x = REF_W / 2;
    m->state.cursor_y = REF_H / 2;
    m->state.cursor_visible = 0;
    t9_reset(&m->t9);

#if !defined(PHONE_DESKTOP) && !defined(PHONE_VFB)
    open_console(m);

    m->epfd = epoll_create1(EPOLL_CLOEXEC);
    if (m->epfd < 0)
        m->epfd = epoll_create1(0);
    if (m->epfd < 0)
        return -1;

    m->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m->inotify_fd < 0)
        m->inotify_fd = inotify_init1(IN_NONBLOCK);
    if (m->inotify_fd >= 0) {
        if (inotify_add_watch(m->inotify_fd, "/dev/input", IN_CREATE | IN_ATTRIB | IN_DELETE) >= 0) {
            struct epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.events = EPOLLIN;
            ev.data.fd = m->inotify_fd;
            epoll_ctl(m->epfd, EPOLL_CTL_ADD, m->inotify_fd, &ev);
        }
    }
    scan_devices(m);
#elif defined(PHONE_VFB) && !defined(_WIN32)
    if (isatty(STDIN_FILENO)) {
        tcgetattr(STDIN_FILENO, &g_old_term);
        struct termios t = g_old_term;
        t.c_lflag &= ~(ICANON | ECHO);
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
        g_term_set = 1;
    }
#endif
    return 0;
}

void input_shutdown(InputMux *m)
{
#if !defined(PHONE_DESKTOP) && !defined(PHONE_VFB)
    for (int i = 0; i < m->fd_count; i++) {
        if (m->grabbed[i])
            ioctl(m->fds[i], EVIOCGRAB, 0);
        close(m->fds[i]);
    }
    m->fd_count = 0;
    if (m->inotify_fd >= 0)
        close(m->inotify_fd);
    if (m->epfd >= 0)
        close(m->epfd);
    if (m->tty_fd >= 0) {
        console_graphics(m, 0);
        close(m->tty_fd);
    }
#elif defined(PHONE_VFB) && !defined(_WIN32)
    if (g_term_set) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_old_term);
        g_term_set = 0;
    }
#endif
    m->inotify_fd = -1;
    m->epfd = -1;
    m->tty_fd = -1;
}

void input_mode_label(const InputMux *m, char *buf, size_t n)
{
    if (!buf || n == 0)
        return;
    if (!m) {
        str_copy(buf, n, "[T9: abc]");
        return;
    }
    if (m->dialer_mode) {
        str_copy(buf, n, "[123]");
        return;
    }
    if (m->state.last_qwerty) {
        str_copy(buf, n, "[QWERTY]");
        return;
    }
    if (m->t9.upper)
        str_copy(buf, n, "[T9: ABC]");
    else
        str_copy(buf, n, "[T9: abc]");
}

void input_set_dialer_mode(InputMux *m, int enabled)
{
    m->dialer_mode = enabled;
    m->state.last_qwerty = 0;
    t9_reset(&m->t9);
}

static void show_cursor(InputMux *m)
{
    m->state.cursor_visible = 1;
    m->state.last_pointer_ms = now_ms();
}

static void hide_cursor_if_idle(InputMux *m)
{
    if (!m->state.cursor_visible)
        return;
    if (now_ms() - m->state.last_pointer_ms > 4000 &&
        now_ms() - m->state.last_key_ms < 4000) {
        m->state.cursor_visible = 0;
    }
}

static int map_digit(int code)
{
    if (code >= KEY_1 && code <= KEY_9)
        return code - KEY_1 + 1;
    if (code == KEY_0)
        return 0;
    if (code >= KEY_KP1 && code <= KEY_KP9)
        return code - KEY_KP1 + 1;
    if (code == KEY_KP0)
        return 0;
    return -1;
}

static int letter_from_key(int code)
{
    switch (code) {
    case KEY_A: return 'a';
    case KEY_B: return 'b';
    case KEY_C: return 'c';
    case KEY_D: return 'd';
    case KEY_E: return 'e';
    case KEY_F: return 'f';
    case KEY_G: return 'g';
    case KEY_H: return 'h';
    case KEY_I: return 'i';
    case KEY_J: return 'j';
    case KEY_K: return 'k';
    case KEY_L: return 'l';
    case KEY_M: return 'm';
    case KEY_N: return 'n';
    case KEY_O: return 'o';
    case KEY_P: return 'p';
    case KEY_Q: return 'q';
    case KEY_R: return 'r';
    case KEY_S: return 's';
    case KEY_T: return 't';
    case KEY_U: return 'u';
    case KEY_V: return 'v';
    case KEY_W: return 'w';
    case KEY_X: return 'x';
    case KEY_Y: return 'y';
    case KEY_Z: return 'z';
    default: return 0;
    }
}

static char key_to_char(InputMux *m, int code)
{
    int letter = letter_from_key(code);
    if (letter) {
        char c = (char)letter;
        if (m->state.shift || m->state.caps)
            c = (char)(c - 'a' + 'A');
        return c;
    }
    int shift = m->state.shift;
    switch (code) {
    case KEY_1: return shift ? '!' : '1';
    case KEY_2: return shift ? '@' : '2';
    case KEY_3: return shift ? '#' : '3';
    case KEY_4: return shift ? '$' : '4';
    case KEY_5: return shift ? '%' : '5';
    case KEY_6: return shift ? '^' : '6';
    case KEY_7: return shift ? '&' : '7';
    case KEY_8: return shift ? '*' : '8';
    case KEY_9: return shift ? '(' : '9';
    case KEY_0: return shift ? ')' : '0';
    case KEY_MINUS: return shift ? '_' : '-';
    case KEY_EQUAL: return shift ? '+' : '=';
    case KEY_LEFTBRACE: return shift ? '{' : '[';
    case KEY_RIGHTBRACE: return shift ? '}' : ']';
    case KEY_SEMICOLON: return shift ? ':' : ';';
    case KEY_APOSTROPHE: return shift ? '"' : '\'';
    case KEY_GRAVE: return shift ? '~' : '`';
    case KEY_BACKSLASH: return shift ? '|' : '\\';
    case KEY_COMMA: return shift ? '<' : ',';
    case KEY_DOT: return shift ? '>' : '.';
    case KEY_SLASH: return shift ? '?' : '/';
    case KEY_SPACE: return ' ';
    default: return 0;
    }
}

static int handle_key(InputMux *m, int code, int value, InputEvent *out)
{
    if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL) {
        m->state.ctrl = (value != 0);
        return 0;
    }
    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
        m->state.shift = (value != 0);
        if (value == 0)
            return 0;
    }
    if (value != 1 && value != 2)
        return 0;

    if (code == KEY_CAPSLOCK && value == 1) {
        m->state.caps = !m->state.caps;
        return 0;
    }

    m->state.last_key_ms = now_ms();
    if (now_ms() - m->state.last_pointer_ms > 100)
        m->state.cursor_visible = 0;

    if (code == KEY_HOME || code == KEY_LEFTMETA || code == KEY_RIGHTMETA ||
        (code == KEY_H && m->state.ctrl)) {
        out->kind = IE_HOME;
        return 1;
    }
    if (code == KEY_UP) {
        out->kind = IE_NAV_UP;
        return 1;
    }
    if (code == KEY_DOWN) {
        out->kind = IE_NAV_DOWN;
        return 1;
    }
    if (code == KEY_LEFT) {
        out->kind = IE_NAV_LEFT;
        return 1;
    }
    if (code == KEY_RIGHT) {
        out->kind = IE_NAV_RIGHT;
        return 1;
    }
    if (code == KEY_ENTER || code == KEY_KPENTER) {
        out->kind = IE_OK;
        return 1;
    }
    if (code == KEY_ESC || code == KEY_BACK) {
        out->kind = IE_BACK;
        return 1;
    }
    if (code == KEY_F1) {
        out->kind = IE_SOFT_LEFT;
        return 1;
    }
    if (code == KEY_BACKSPACE) {
        out->kind = IE_BACKSPACE;
        return 1;
    }
    if (code == KEY_F2 || code == KEY_F3) {
        out->kind = IE_SOFT_RIGHT;
        return 1;
    }

    if (code == KEY_KPASTERISK) {
        int replace = 0;
        char c = t9_symbol(&m->t9, now_ms(), &replace);
        out->kind = IE_CHAR;
        out->ch = c;
        out->x = replace;
        return 1;
    }
    if (code == KEY_KPSLASH) {
        t9_toggle_case(&m->t9);
        if (m->t9.len) {
            out->kind = IE_CHAR;
            out->ch = m->t9.buf[0];
            out->x = 1;
            return 1;
        }
        return 0;
    }

    char q = key_to_char(m, code);
    if (q && letter_from_key(code)) {
        t9_reset(&m->t9);
        m->state.last_qwerty = 1;
        out->kind = IE_CHAR;
        out->ch = q;
        out->x = 0;
        m->state.shift = 0;
        return 1;
    }
    if (q && (code == KEY_SPACE || code == KEY_COMMA || code == KEY_DOT ||
              code == KEY_SLASH || code == KEY_SEMICOLON || code == KEY_APOSTROPHE ||
              code == KEY_MINUS || code == KEY_EQUAL || code == KEY_LEFTBRACE ||
              code == KEY_RIGHTBRACE || code == KEY_BACKSLASH || code == KEY_GRAVE)) {
        t9_reset(&m->t9);
        m->state.last_qwerty = 1;
        out->kind = IE_CHAR;
        out->ch = q;
        out->x = 0;
        m->state.shift = 0;
        return 1;
    }

    int digit = map_digit(code);
    if (digit >= 0) {
        if (m->dialer_mode) {
            t9_reset(&m->t9);
            m->state.last_qwerty = 0;
            out->kind = IE_CHAR;
            out->ch = (char)('0' + digit);
            out->x = 0;
            return 1;
        }
        if (m->state.shift && code >= KEY_0 && code <= KEY_9) {
            char c = key_to_char(m, code);
            t9_reset(&m->t9);
            m->state.last_qwerty = 1;
            out->kind = IE_CHAR;
            out->ch = c;
            out->x = 0;
            m->state.shift = 0;
            return 1;
        }
        m->state.last_qwerty = 0;
        int replace = 0;
        char c = t9_feed(&m->t9, digit, now_ms(), &replace);
        out->kind = IE_CHAR;
        out->ch = c;
        out->x = replace;
        return 1;
    }

    return 0;
}

#ifdef PHONE_DESKTOP
static int poll_sdl(InputMux *m, InputEvent *out)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            out->kind = IE_QUIT;
            return 1;
        }
        if (e.type == SDL_KEYDOWN) {
            int code = 0;
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_UP) code = KEY_UP;
            else if (k == SDLK_DOWN) code = KEY_DOWN;
            else if (k == SDLK_LEFT) code = KEY_LEFT;
            else if (k == SDLK_RIGHT) code = KEY_RIGHT;
            else if (k == SDLK_RETURN) code = KEY_ENTER;
            else if (k == SDLK_ESCAPE) code = KEY_ESC;
            else if (k == SDLK_BACKSPACE) code = KEY_BACKSPACE;
            else if (k == SDLK_HOME) code = KEY_HOME;
            else if (k == SDLK_LGUI || k == SDLK_RGUI) code = KEY_LEFTMETA;
            else if (k == SDLK_LCTRL || k == SDLK_RCTRL) code = KEY_LEFTCTRL;
            else if (k == SDLK_F1) code = KEY_F1;
            else if (k == SDLK_F2) code = KEY_F2;
            else if (k == SDLK_SPACE) code = KEY_SPACE;
            else if (k == SDLK_LSHIFT || k == SDLK_RSHIFT) code = KEY_LEFTSHIFT;
            else if (k >= SDLK_a && k <= SDLK_z) {
                static const int letters[] = {
                    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
                    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
                    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z
                };
                code = letters[k - SDLK_a];
            } else if (k == SDLK_0) code = KEY_0;
            else if (k >= SDLK_1 && k <= SDLK_9) code = KEY_1 + (int)(k - SDLK_1);
            else if (k == SDLK_PERIOD) code = KEY_DOT;
            else if (k == SDLK_COMMA) code = KEY_COMMA;
            else if (k == SDLK_SLASH) code = KEY_SLASH;
            else if (k == SDLK_ASTERISK || k == SDLK_KP_MULTIPLY) code = KEY_KPASTERISK;
            else if (k == SDLK_HASH || k == SDLK_KP_DIVIDE) code = KEY_KPSLASH;
            if (e.key.keysym.mod & KMOD_SHIFT)
                m->state.shift = 1;
            if (code && handle_key(m, code, 1, out))
                return 1;
        }
        if (e.type == SDL_KEYUP) {
            if (e.key.keysym.sym == SDLK_LSHIFT || e.key.keysym.sym == SDLK_RSHIFT)
                m->state.shift = 0;
        }
        if (e.type == SDL_MOUSEMOTION) {
            int bw = m->state.bound_w > 0 ? m->state.bound_w : REF_W;
            int bh = m->state.bound_h > 0 ? m->state.bound_h : REF_H;
            m->state.cursor_x = clamp_i(e.motion.x, 0, bw - 1);
            m->state.cursor_y = clamp_i(e.motion.y, 0, bh - 1);
            show_cursor(m);
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            int bw = m->state.bound_w > 0 ? m->state.bound_w : REF_W;
            int bh = m->state.bound_h > 0 ? m->state.bound_h : REF_H;
            m->state.cursor_x = clamp_i(e.button.x, 0, bw - 1);
            m->state.cursor_y = clamp_i(e.button.y, 0, bh - 1);
            show_cursor(m);
            out->kind = IE_CLICK;
            out->x = m->state.cursor_x;
            out->y = m->state.cursor_y;
            return 1;
        }
    }
    return 0;
}
#endif

#ifdef PHONE_VFB
static int poll_stdin(InputMux *m, InputEvent *out)
{
#ifdef _WIN32
    if (!_kbhit())
        return 0;
    int ch = _getch();
    if (ch == 0 || ch == 224) {
        int a = _getch();
        if (a == 72) return handle_key(m, KEY_UP, 1, out);
        if (a == 80) return handle_key(m, KEY_DOWN, 1, out);
        if (a == 75) return handle_key(m, KEY_LEFT, 1, out);
        if (a == 77) return handle_key(m, KEY_RIGHT, 1, out);
        if (a == 59) return handle_key(m, KEY_F1, 1, out);
        if (a == 60) return handle_key(m, KEY_F2, 1, out);
        return 0;
    }
    if (ch == 27) return handle_key(m, KEY_ESC, 1, out);
    if (ch == 13) return handle_key(m, KEY_ENTER, 1, out);
    if (ch == 8) return handle_key(m, KEY_BACKSPACE, 1, out);
    if (ch == '*') return handle_key(m, KEY_KPASTERISK, 1, out);
    if (ch == '#' || ch == '/') return handle_key(m, KEY_KPSLASH, 1, out);
    if (ch == ' ') return handle_key(m, KEY_SPACE, 1, out);
    if (ch >= '0' && ch <= '9') {
        int code = (ch == '0') ? KEY_0 : (KEY_1 + (ch - '1'));
        return handle_key(m, code, 1, out);
    }
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
        static const int letters[] = {
            KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
            KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
            KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z
        };
        int up = (ch >= 'A' && ch <= 'Z');
        int idx = up ? (ch - 'A') : (ch - 'a');
        if (up)
            m->state.shift = 1;
        int r = handle_key(m, letters[idx], 1, out);
        m->state.shift = 0;
        return r;
    }
    return 0;
#else
    unsigned char buf[8];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0)
        return 0;
    if (buf[0] == 0x1b && n >= 3 && buf[1] == '[') {
        if (buf[2] == 'A') return handle_key(m, KEY_UP, 1, out);
        if (buf[2] == 'B') return handle_key(m, KEY_DOWN, 1, out);
        if (buf[2] == 'C') return handle_key(m, KEY_RIGHT, 1, out);
        if (buf[2] == 'D') return handle_key(m, KEY_LEFT, 1, out);
    }
    if (buf[0] == 0x1b && n == 1) return handle_key(m, KEY_ESC, 1, out);
    if (buf[0] == '\n' || buf[0] == '\r') return handle_key(m, KEY_ENTER, 1, out);
    if (buf[0] == 127 || buf[0] == 8) return handle_key(m, KEY_BACKSPACE, 1, out);
    if (buf[0] == '*') return handle_key(m, KEY_KPASTERISK, 1, out);
    if (buf[0] == '#' || buf[0] == '/') return handle_key(m, KEY_KPSLASH, 1, out);
    if (buf[0] == ' ') return handle_key(m, KEY_SPACE, 1, out);
    if (buf[0] >= '0' && buf[0] <= '9') {
        int code = (buf[0] == '0') ? KEY_0 : (KEY_1 + (buf[0] - '1'));
        return handle_key(m, code, 1, out);
    }
    if ((buf[0] >= 'a' && buf[0] <= 'z') || (buf[0] >= 'A' && buf[0] <= 'Z')) {
        static const int letters[] = {
            KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
            KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
            KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z
        };
        int up = (buf[0] >= 'A' && buf[0] <= 'Z');
        int idx = up ? (buf[0] - 'A') : (buf[0] - 'a');
        if (up)
            m->state.shift = 1;
        int r = handle_key(m, letters[idx], 1, out);
        m->state.shift = 0;
        return r;
    }
    return 0;
#endif
}
#endif

int input_poll(InputMux *m, InputEvent *out, int timeout_ms)
{
    memset(out, 0, sizeof(*out));
    hide_cursor_if_idle(m);

#ifdef PHONE_DESKTOP
    (void)timeout_ms;
    if (poll_sdl(m, out))
        return 1;
    if (m->t9.len > 0 && !t9_pending(&m->t9, now_ms()))
        t9_commit(&m->t9);
    return 0;
#elif defined(PHONE_VFB)
    (void)timeout_ms;
#ifdef _WIN32
    Sleep(timeout_ms > 0 ? (DWORD)timeout_ms : 1);
#else
    usleep((useconds_t)(timeout_ms > 0 ? timeout_ms * 1000 : 1000));
#endif
    if (poll_stdin(m, out))
        return 1;
    if (m->t9.len > 0 && !t9_pending(&m->t9, now_ms()))
        t9_commit(&m->t9);
    return 0;
#else
    if (m->t9.len > 0 && !t9_pending(&m->t9, now_ms()))
        t9_commit(&m->t9);

    if (m->epfd < 0)
        return 0;

    if (m->fd_count == 0)
        scan_devices(m);

    struct epoll_event events[8];
    int n = epoll_wait(m->epfd, events, 8, timeout_ms);
    if (n < 0)
        return 0;

    for (int i = 0; i < n; i++) {
        int fd = events[i].data.fd;
        if (fd == m->inotify_fd) {
            char buf[512];
            while (read(m->inotify_fd, buf, sizeof(buf)) > 0) {
            }
            scan_devices(m);
            continue;
        }

        struct input_event iev;
        while (read(fd, &iev, sizeof(iev)) == (ssize_t)sizeof(iev)) {
            if (iev.type == EV_KEY) {
                if (iev.code == KEY_LEFTSHIFT || iev.code == KEY_RIGHTSHIFT) {
                    if (iev.value == 0)
                        m->state.shift = 0;
                }
                if (iev.code == BTN_LEFT && iev.value == 1) {
                    show_cursor(m);
                    out->kind = IE_CLICK;
                    out->x = m->state.cursor_x;
                    out->y = m->state.cursor_y;
                    return 1;
                }
                if (handle_key(m, (int)iev.code, (int)iev.value, out))
                    return 1;
            } else if (iev.type == EV_REL) {
                int bw = m->state.bound_w > 0 ? m->state.bound_w : REF_W;
                int bh = m->state.bound_h > 0 ? m->state.bound_h : REF_H;
                if (iev.code == REL_X)
                    m->state.cursor_x = clamp_i(m->state.cursor_x + (int)iev.value, 0, bw - 1);
                else if (iev.code == REL_Y)
                    m->state.cursor_y = clamp_i(m->state.cursor_y + (int)iev.value, 0, bh - 1);
                show_cursor(m);
            } else if (iev.type == EV_ABS) {
                int bw = m->state.bound_w > 0 ? m->state.bound_w : REF_W;
                int bh = m->state.bound_h > 0 ? m->state.bound_h : REF_H;
                if (iev.code == ABS_X || iev.code == ABS_MT_POSITION_X)
                    m->state.cursor_x = clamp_i((int)iev.value, 0, bw - 1);
                else if (iev.code == ABS_Y || iev.code == ABS_MT_POSITION_Y)
                    m->state.cursor_y = clamp_i((int)iev.value, 0, bh - 1);
                show_cursor(m);
            }
        }
        if (errno == ENODEV)
            remove_fd(m, fd);
    }
    return 0;
#endif
}
