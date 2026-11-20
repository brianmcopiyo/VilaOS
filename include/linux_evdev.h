#ifndef VILAOS_LINUX_EVDEV_H
#define VILAOS_LINUX_EVDEV_H

#include "input.h"

int linux_evdev_open(const char *path, void **out_dev, int *out_fd);
void linux_evdev_close(void *dev);
int linux_evdev_read(void *dev, InputEvent *out, InputMux *m);

#endif
