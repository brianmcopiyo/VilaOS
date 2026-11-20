#include "chromium_launch.h"
#include "linux_gfx.h"
#include "linux_gpu.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

static pid_t g_chrome_pid = -1;

static int exists_exec(const char *path)
{
    return path && access(path, X_OK) == 0;
}

static const char *find_chromium(char *buf, size_t n)
{
    static const char *cands[] = {
        "/opt/chromium/chrome",
        "/opt/chromium/chromium",
        "/mnt/iso/chromium/chrome",
        "/mnt/iso/chromium/chromium",
        "/opt/chromium/chromium/chrome",
        NULL
    };
    for (int i = 0; cands[i]; i++) {
        if (exists_exec(cands[i])) {
            str_copy(buf, n, cands[i]);
            return buf;
        }
    }
    return NULL;
}

int chromium_available(void)
{
    char bin[256];
    if (exists_exec("/bin/vilaos-chromium"))
        return 1;
    return find_chromium(bin, sizeof(bin)) ? 1 : 0;
}

int chromium_release_display(Gfx *g)
{
    if (!g)
        return -1;
    if (getenv("VILAOS_HEADLESS_UI") || str_eq(g->backend, "headless")) {
        g->dri_fd = -1;
        g->fb_fd = -1;
        g->fb_map = NULL;
        g->fb_map_len = 0;
        return 0;
    }
#if defined(VILAOS_HAVE_LINUX_GFX)
    if (linux_gpu_active(g))
        linux_gpu_close(g);
    else if (g->gfx_tier == GFX_TIER_DRM || g->dri_fd >= 0)
        linux_drm_close(g);
#endif
    if (g->fb_map && g->fb_map_len) {
#if !defined(PHONE_DESKTOP) && !defined(PHONE_VFB)
        munmap(g->fb_map, g->fb_map_len);
#endif
        g->fb_map = NULL;
        g->fb_map_len = 0;
    }
    if (g->fb_fd >= 0) {
        close(g->fb_fd);
        g->fb_fd = -1;
    }
    if (g->dri_fd >= 0) {
        close(g->dri_fd);
        g->dri_fd = -1;
    }
    return 0;
}

int chromium_launch(const char *url)
{
    if (g_chrome_pid > 0)
        return 0;
    if (!chromium_available()) {
        fprintf(stderr, "chromium: binary not found\n");
        return -1;
    }
    const char *page = (url && url[0]) ? url : "file:///ui/index.html";
    mkdir("/run/vilaos", 0755);
    fprintf(stderr, "chromium: forking launcher\n");
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "chromium: fork errno=%d\n", errno);
        fflush(stderr);
        return -1;
    }
    if (pid == 0) {
        int logfd = open("/run/vilaos/chromium.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (logfd >= 0) {
            dup2(logfd, 1);
            dup2(logfd, 2);
            if (logfd > 2)
                close(logfd);
        }
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, 0);
            if (devnull > 2)
                close(devnull);
        }
        setsid();
        if (exists_exec("/bin/vilaos-chromium")) {
            fprintf(stderr, "chromium: wrapper /bin/vilaos-chromium %s\n", page);
            execl("/bin/vilaos-chromium", "vilaos-chromium", page, (char *)NULL);
        }
        char bin[256];
        find_chromium(bin, sizeof(bin));
        char appflag[256];
        snprintf(appflag, sizeof(appflag), "--app=%s", page);
        setenv("HOME", "/tmp", 1);
        setenv("XDG_CONFIG_HOME", "/tmp/chromium-config", 1);
        setenv("XDG_CACHE_HOME", "/tmp/chromium-cache", 1);
        setenv("XDG_RUNTIME_DIR", "/run/user/0", 1);
        {
            char ld[512];
            const char *old = getenv("LD_LIBRARY_PATH");
            snprintf(ld, sizeof(ld),
                     "/opt/chromium:/opt/chromium/lib:/opt/chromium/lib64:/mnt/iso/chromium:/mnt/iso/chromium/lib:/mnt/iso/chromium/lib64:%s",
                     old ? old : "/lib");
            setenv("LD_LIBRARY_PATH", ld, 1);
        }
        chdir("/opt/chromium");
        char *args[] = {
            bin,
            "--kiosk",
            appflag,
            "--ozone-platform=wayland",
            "--enable-features=UseOzonePlatform",
            "--use-gl=angle",
            "--use-angle=swiftshader",
            "--ignore-gpu-blocklist",
            "--allow-file-access-from-files",
            "--disable-web-security",
            "--disable-dev-shm-usage",
            "--no-sandbox",
            "--no-zygote",
            "--no-first-run",
            "--enable-logging=stderr",
            "--user-data-dir=/tmp/chromium-data",
            NULL
        };
        fprintf(stderr, "chromium: direct exec %s %s\n", bin, page);
        execv(bin, args);
        fprintf(stderr, "chromium: exec failed errno=%d\n", errno);
        _exit(127);
    }
    g_chrome_pid = pid;
    fprintf(stderr, "chromium: launched pid=%d url=%s\n", (int)pid, page);
    return 0;
}

int chromium_running(void)
{
    if (g_chrome_pid <= 0)
        return 0;
    int status = 0;
    pid_t r = waitpid(g_chrome_pid, &status, WNOHANG);
    if (r == g_chrome_pid) {
        fprintf(stderr, "chromium: exited status=%d\n", status);
        g_chrome_pid = -1;
        return 0;
    }
    return 1;
}

void chromium_reap(void)
{
    (void)chromium_running();
}

#else

int chromium_available(void)
{
    return 0;
}
int chromium_release_display(Gfx *g)
{
    (void)g;
    return -1;
}
int chromium_launch(const char *url)
{
    (void)url;
    return -1;
}
int chromium_running(void)
{
    return 0;
}
void chromium_reap(void)
{
}

#endif
