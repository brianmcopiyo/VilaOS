#include "syspower.h"
#include "util.h"
#include "fb.h"
#include "screen.h"
#include "procman.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#endif

static void persist_and_sync(void)
{
    procman_stop_all(&g_procman);
#ifndef _WIN32
    sync();
    umount2("/data", MNT_DETACH);
    umount2("/var", MNT_DETACH);
    umount2("/run", MNT_DETACH);
    sync();
#endif
}

void syspower_show_message(Screen *s, Gfx *g, InputMux *input, const char *msg)
{
    if (!s || !g)
        return;
    if (screen_load(s, "assets/screens/shutdown.xml") != 0)
        return;
    Widget *m = screen_find(s, "shut_msg");
    if (m && msg)
        str_copy(m->text, sizeof(m->text), msg);
    screen_layout(s, g);
    screen_render(s, g, input ? &input->state : NULL);
    gfx_present(g);
#ifndef _WIN32
    usleep(900000);
#else
    (void)msg;
#endif
}

void syspower_off(Screen *s, Gfx *g, InputMux *input)
{
    syspower_show_message(s, g, input, "Shutting down VilaOS...");
    persist_and_sync();
#ifndef _WIN32
    reboot(LINUX_REBOOT_CMD_POWER_OFF);
    for (;;)
        sleep(5);
#else
    (void)s;
    (void)g;
    (void)input;
#endif
}

void syspower_restart(Screen *s, Gfx *g, InputMux *input)
{
    syspower_show_message(s, g, input, "Restarting VilaOS...");
    persist_and_sync();
#ifndef _WIN32
    reboot(LINUX_REBOOT_CMD_RESTART);
    for (;;)
        sleep(5);
#else
    (void)s;
    (void)g;
    (void)input;
#endif
}
