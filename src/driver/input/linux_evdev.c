#include "linux_evdev.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(VILAOS_HAVE_LINUX_GFX) && !defined(PHONE_DESKTOP) && !defined(PHONE_VFB) && !defined(_WIN32)

#include <fcntl.h>
#include <unistd.h>
#include <libevdev/libevdev.h>

int linux_evdev_open(const char *path, void **out_dev, int *out_fd)
{
    if (!path || !out_dev || !out_fd)
        return -1;
    *out_dev = NULL;
    *out_fd = -1;
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return -1;
    struct libevdev *dev = NULL;
    if (libevdev_new_from_fd(fd, &dev) < 0) {
        close(fd);
        return -1;
    }
    *out_dev = dev;
    *out_fd = fd;
    return 0;
}

void linux_evdev_close(void *dev)
{
    if (!dev)
        return;
    int fd = libevdev_get_fd((struct libevdev *)dev);
    libevdev_free((struct libevdev *)dev);
    if (fd >= 0)
        close(fd);
}

int linux_evdev_read(void *dev, InputEvent *out, InputMux *m)
{
    if (!dev || !out || !m)
        return 0;
    memset(out, 0, sizeof(*out));
    struct input_event ev;
    int rc = libevdev_next_event((struct libevdev *)dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    if (rc == LIBEVDEV_READ_STATUS_SYNC) {
        while (rc == LIBEVDEV_READ_STATUS_SYNC)
            rc = libevdev_next_event((struct libevdev *)dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
    }
    if (rc != LIBEVDEV_READ_STATUS_SUCCESS)
        return 0;
    int bw = m->state.bound_w > 0 ? m->state.bound_w : REF_W;
    int bh = m->state.bound_h > 0 ? m->state.bound_h : REF_H;
    if (ev.type == EV_REL) {
        if (ev.code == REL_X)
            m->state.cursor_x = clamp_i(m->state.cursor_x + (int)ev.value, 0, bw - 1);
        else if (ev.code == REL_Y)
            m->state.cursor_y = clamp_i(m->state.cursor_y + (int)ev.value, 0, bh - 1);
        m->state.cursor_visible = 1;
        m->state.last_pointer_ms = now_ms();
        m->pointer_gen++;
        return 0;
    }
    if (ev.type == EV_KEY && ev.value == 1 && (ev.code == BTN_LEFT || ev.code == BTN_TOUCH)) {
        out->kind = IE_CLICK;
        out->x = m->state.cursor_x;
        out->y = m->state.cursor_y;
        m->state.cursor_visible = 1;
        m->state.last_pointer_ms = now_ms();
        return 1;
    }
    if (ev.type == EV_ABS) {
        if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) {
            int minv = libevdev_get_abs_minimum((struct libevdev *)dev, ev.code);
            int maxv = libevdev_get_abs_maximum((struct libevdev *)dev, ev.code);
            if (maxv > minv)
                m->state.cursor_x = clamp_i((int)(((int64_t)(ev.value - minv) * (bw - 1)) / (maxv - minv)), 0, bw - 1);
            m->state.cursor_visible = 1;
            m->state.last_pointer_ms = now_ms();
            m->pointer_gen++;
        } else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) {
            int minv = libevdev_get_abs_minimum((struct libevdev *)dev, ev.code);
            int maxv = libevdev_get_abs_maximum((struct libevdev *)dev, ev.code);
            if (maxv > minv)
                m->state.cursor_y = clamp_i((int)(((int64_t)(ev.value - minv) * (bh - 1)) / (maxv - minv)), 0, bh - 1);
            m->state.cursor_visible = 1;
            m->state.last_pointer_ms = now_ms();
            m->pointer_gen++;
        }
    }
    return 0;
}

#else

int linux_evdev_open(const char *path, void **out_dev, int *out_fd)
{
    (void)path;
    if (out_dev)
        *out_dev = NULL;
    if (out_fd)
        *out_fd = -1;
    return -1;
}

void linux_evdev_close(void *dev)
{
    (void)dev;
}

int linux_evdev_read(void *dev, InputEvent *out, InputMux *m)
{
    (void)dev;
    (void)out;
    (void)m;
    return 0;
}

#endif
