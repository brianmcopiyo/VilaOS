#include "types.h"
#include "util.h"
#include "fb.h"
#include "pkg.h"
#include "procman.h"
#include "vapi.h"
#include "window.h"
#include "webbridge.h"
#include "chromium_launch.h"
#include "apprun.h"
#include "hwinfo.h"
#include "screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#else
static int setenv(const char *n, const char *v, int o)
{
    (void)o;
    return _putenv_s(n, v);
}
#endif

static volatile int g_run = 1;
static AppHost g_host;
static WebBridge *g_web = NULL;
static int g_pid1;
static int g_chrome_fail = 0;
static int running_as_pid1(void)
{
#ifdef _WIN32
    return 0;
#else
    return getpid() == 1;
#endif
}

static void hang_forever(void)
{
#ifndef _WIN32
    for (;;)
        sleep(5);
#else
    for (;;)
        ;
#endif
}

static void dump_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "%s: missing\n", path);
        return;
    }
    char line[256];
    fprintf(stderr, "%s:\n", path);
    while (fgets(line, sizeof(line), f))
        fputs(line, stderr);
    fclose(f);
}

static void start_musicd(void)
{
#ifndef _WIN32
    if (access("/bin/vilaos-musicd", X_OK) != 0)
        return;
    pid_t mpid = fork();
    if (mpid == 0) {
        execl("/bin/vilaos-musicd", "vilaos-musicd", (char *)NULL);
        _exit(127);
    }
#else
    (void)0;
#endif
}

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

static int bring_up_loopback(void)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        fprintf(stderr, "net: AF_INET socket errno=%d (need CONFIG_INET)\n", errno);
        return -1;
    }
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    str_copy(ifr.ifr_name, sizeof(ifr.ifr_name), "lo");
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (ioctl(fd, SIOCSIFADDR, &ifr) != 0)
        fprintf(stderr, "net: SIOCSIFADDR lo errno=%d\n", errno);
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) != 0) {
        fprintf(stderr, "net: SIOCGIFFLAGS lo errno=%d\n", errno);
        close(fd);
        return -1;
    }
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0) {
        fprintf(stderr, "net: SIOCSIFFLAGS lo errno=%d\n", errno);
        close(fd);
        return -1;
    }
    close(fd);
    fprintf(stderr, "net: loopback 127.0.0.1 up\n");
    return 0;
}
#endif

static int ensure_chromium_ui(Gfx *gfx)
{
    if (!chromium_available()) {
        fprintf(stderr, "chromium: binary missing\n");
        dump_file("/run/vilaos/chromium.status");
        return -1;
    }
    if (chromium_running())
        return 0;
    fprintf(stderr, "chromium: releasing display for sole UI\n");
    fflush(stderr);
    chromium_release_display(gfx);
    fprintf(stderr, "chromium: spawning file:///ui/index.html\n");
    fflush(stderr);
#ifndef _WIN32
    usleep(50000);
#endif
    if (chromium_launch("file:///ui/index.html") != 0) {
        fprintf(stderr, "chromium: launch failed\n");
        fflush(stderr);
        dump_file("/run/vilaos/chromium.log");
        return -1;
    }
#ifndef _WIN32
    for (int i = 0; i < 40; i++) {
        webbridge_poll(g_web);
        usleep(50000);
        if (!chromium_running())
            break;
        if (i >= 6)
            break;
    }
#endif
    if (!chromium_running()) {
        fprintf(stderr, "chromium: exited immediately\n");
        dump_file("/run/vilaos/chromium.log");
        return -1;
    }
    fprintf(stderr, "chromium: graphical UI active\n");
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    g_pid1 = running_as_pid1();
#ifndef _WIN32
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
#endif

    setenv("VILAOS_HEADLESS_UI", "1", 1);

    Gfx gfx;
    memset(&gfx, 0, sizeof(gfx));
    gfx.fb_fd = -1;
    gfx.dri_fd = -1;
    if (gfx_init(&gfx, 0) != 0) {
        fprintf(stderr, "gfx_init(headless) failed\n");
        if (g_pid1)
            hang_forever();
        return 1;
    }
    {
        char label[192];
        gfx_format_label(&gfx, label, sizeof(label));
        fprintf(stderr, "gfx(headless): %s\n", label);
    }

    vapi_init();
    apphost_init(&g_host);
    procman_init(&g_procman);
    g_host.unlocked = 1;

    Screen screen;
    memset(&screen, 0, sizeof(screen));
    screen_init(&screen);
    wm_init(&g_wm, &screen, &gfx);
    wm_set_work_area(&g_wm, 0, 0, gfx.w > 0 ? gfx.w : 1280, gfx.h > 48 ? gfx.h - 48 : 720);

    if (g_pid1)
        start_musicd();

    dump_file("/run/vilaos/chromium.status");

#ifndef _WIN32
    if (bring_up_loopback() != 0)
        fprintf(stderr, "net: loopback bring-up failed errno may block ipc\n");
#endif

    if (webbridge_start(&g_web, &gfx, &g_wm, 9081) != 0) {
        fprintf(stderr, "ipc: failed to bind :9081\n");
        if (g_pid1)
            hang_forever();
        return 1;
    }
    webbridge_set_root(g_web, "/ui");
    fprintf(stderr, "ipc: runtime socket ready\n");

    if (ensure_chromium_ui(&gfx) != 0) {
        fprintf(stderr, "chromium: required for display; retrying\n");
    }

    while (g_run) {
        webbridge_poll(g_web);
        chromium_reap();
        if (!chromium_running()) {
            if (g_chrome_fail >= 5) {
                static int dumped = 0;
                if (!dumped) {
                    fprintf(stderr, "chromium: stopped after %d consecutive failures\n", g_chrome_fail);
                    dump_file("/run/vilaos/chromium.log");
                    dumped = 1;
                }
#ifndef _WIN32
                sleep(5);
#endif
                continue;
            }
            fprintf(stderr, "chromium: respawning attempt=%d\n", g_chrome_fail + 1);
            dump_file("/run/vilaos/chromium.log");
#ifndef _WIN32
            sleep(2);
#endif
            (void)ensure_chromium_ui(&gfx);
            if (chromium_running())
                g_chrome_fail = 0;
            else
                g_chrome_fail++;
        } else {
            g_chrome_fail = 0;
        }
#ifndef _WIN32
        usleep(20000);
#else
        ;
#endif
        if (!g_pid1 && !chromium_running() && !chromium_available())
            break;
    }

    if (g_web) {
        webbridge_stop(g_web);
        g_web = NULL;
    }
    procman_shutdown(&g_procman);
    gfx_shutdown(&gfx);

    if (g_pid1)
        hang_forever();
    return 0;
}
