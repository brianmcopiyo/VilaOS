#include "webbridge.h"
#include "util.h"
#include "hwinfo.h"
#include "pkg.h"
#include "vapi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

static const char *find_ci(const char *hay, const char *needle)
{
    if (!hay || !needle || !needle[0])
        return NULL;
    size_t n = strlen(needle);
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < n && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (i == n)
            return p;
    }
    return NULL;
}

#if !defined(_WIN32)
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#else
#define WEBBRIDGE_STUB 1
#endif

#ifndef WEBBRIDGE_STUB

#define WB_MAX_CLIENTS 32
#define WB_BUF 16384

typedef struct {
    int fd;
    int is_ws;
    int handshaked;
    char buf[WB_BUF];
    int len;
} WbClient;

struct WebBridge {
    int listen_fd;
    int port;
    Gfx *gfx;
    WindowManager *wm;
    WbClient clients[WB_MAX_CLIENTS];
    char root[MAX_PATH];
    char last_pkg[4096];
    int tick;
};

static int set_nonblock(int fd)
{
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0)
        return -1;
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static void b64_encode(const unsigned char *in, size_t n, char *out, size_t out_n)
{
    static const char *t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < n && o + 4 < out_n; i += 3) {
        unsigned v = in[i] << 16;
        if (i + 1 < n)
            v |= in[i + 1] << 8;
        if (i + 2 < n)
            v |= in[i + 2];
        out[o++] = t[(v >> 18) & 63];
        out[o++] = t[(v >> 12) & 63];
        out[o++] = (i + 1 < n) ? t[(v >> 6) & 63] : '=';
        out[o++] = (i + 2 < n) ? t[v & 63] : '=';
    }
    if (o < out_n)
        out[o] = 0;
}

static void sha1(const unsigned char *data, size_t len, unsigned char out[20])
{
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
    uint64_t bitlen = (uint64_t)len * 8ull;
    unsigned char *msg = (unsigned char *)calloc(1, ((len + 8) / 64 + 1) * 64);
    if (!msg) {
        memset(out, 0, 20);
        return;
    }
    memcpy(msg, data, len);
    msg[len] = 0x80;
    size_t new_len = ((len + 8) / 64 + 1) * 64;
    msg[new_len - 8] = (unsigned char)((bitlen >> 56) & 0xff);
    msg[new_len - 7] = (unsigned char)((bitlen >> 48) & 0xff);
    msg[new_len - 6] = (unsigned char)((bitlen >> 40) & 0xff);
    msg[new_len - 5] = (unsigned char)((bitlen >> 32) & 0xff);
    msg[new_len - 4] = (unsigned char)((bitlen >> 24) & 0xff);
    msg[new_len - 3] = (unsigned char)((bitlen >> 16) & 0xff);
    msg[new_len - 2] = (unsigned char)((bitlen >> 8) & 0xff);
    msg[new_len - 1] = (unsigned char)(bitlen & 0xff);
    for (size_t chunk = 0; chunk < new_len; chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)msg[chunk + i * 4] << 24) | ((uint32_t)msg[chunk + i * 4 + 1] << 16) |
                   ((uint32_t)msg[chunk + i * 4 + 2] << 8) | ((uint32_t)msg[chunk + i * 4 + 3]);
        }
        for (int i = 16; i < 80; i++) {
            uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (x << 1) | (x >> 31);
        }
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }
    free(msg);
    out[0] = (h0 >> 24) & 0xff;
    out[1] = (h0 >> 16) & 0xff;
    out[2] = (h0 >> 8) & 0xff;
    out[3] = h0 & 0xff;
    out[4] = (h1 >> 24) & 0xff;
    out[5] = (h1 >> 16) & 0xff;
    out[6] = (h1 >> 8) & 0xff;
    out[7] = h1 & 0xff;
    out[8] = (h2 >> 24) & 0xff;
    out[9] = (h2 >> 16) & 0xff;
    out[10] = (h2 >> 8) & 0xff;
    out[11] = h2 & 0xff;
    out[12] = (h3 >> 24) & 0xff;
    out[13] = (h3 >> 16) & 0xff;
    out[14] = (h3 >> 8) & 0xff;
    out[15] = h3 & 0xff;
    out[16] = (h4 >> 24) & 0xff;
    out[17] = (h4 >> 16) & 0xff;
    out[18] = (h4 >> 8) & 0xff;
    out[19] = h4 & 0xff;
}

static int ws_accept_key(const char *client_key, char *out, size_t n)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", client_key);
    unsigned char dig[20];
    sha1((const unsigned char *)buf, strlen(buf), dig);
    b64_encode(dig, 20, out, n);
    return 0;
}

static int ws_send_text(int fd, const char *msg)
{
    size_t n = strlen(msg);
    unsigned char hdr[10];
    size_t hlen = 2;
    hdr[0] = 0x81;
    if (n < 126) {
        hdr[1] = (unsigned char)n;
    } else if (n < 65536) {
        hdr[1] = 126;
        hdr[2] = (unsigned char)((n >> 8) & 0xff);
        hdr[3] = (unsigned char)(n & 0xff);
        hlen = 4;
    } else {
        return -1;
    }
    if (write(fd, hdr, hlen) != (ssize_t)hlen)
        return -1;
    if (n && write(fd, msg, n) != (ssize_t)n)
        return -1;
    return 0;
}

static void broadcast(WebBridge *wb, const char *msg)
{
    for (int i = 0; i < WB_MAX_CLIENTS; i++) {
        if (wb->clients[i].fd >= 0 && wb->clients[i].is_ws && wb->clients[i].handshaked)
            ws_send_text(wb->clients[i].fd, msg);
    }
}

static void push_packages(WebBridge *wb)
{
    PkgRegistry reg;
    pkg_registry_load(&reg);
    if (reg.count == 0)
        pkg_seed_from_dir("/apps", &reg);
    if (reg.count == 0)
        pkg_seed_from_dir("/packages", &reg);
    char msg[8192];
    size_t o = 0;
    o += (size_t)snprintf(msg + o, sizeof(msg) - o, "{\"type\":\"packages\",\"apps\":[");
    for (int i = 0; i < reg.count && o + 96 < sizeof(msg); i++) {
        if (i)
            msg[o++] = ',';
        o += (size_t)snprintf(msg + o, sizeof(msg) - o, "{\"id\":\"%s\",\"name\":\"%s\",\"icon\":\"%s\",\"version\":\"%s\"}",
                              reg.apps[i].id, reg.apps[i].name, reg.apps[i].icon, reg.apps[i].version);
    }
    o += (size_t)snprintf(msg + o, sizeof(msg) - o, "]}");
    if (strcmp(msg, wb->last_pkg) != 0) {
        str_copy(wb->last_pkg, sizeof(wb->last_pkg), msg);
        broadcast(wb, msg);
    }
}

static void push_windows(WebBridge *wb)
{
    char msg[4096];
    size_t o = 0;
    o += (size_t)snprintf(msg + o, sizeof(msg) - o, "{\"type\":\"windows\",\"windows\":[");
    int first = 1;
    if (wb->wm) {
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            OsWindow *w = &wb->wm->wins[i];
            if (!w->alive)
                continue;
            if (!first)
                msg[o++] = ',';
            first = 0;
            o += (size_t)snprintf(msg + o, sizeof(msg) - o,
                                  "{\"id\":%d,\"app_id\":\"%s\",\"title\":\"%s\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,"
                                  "\"minimized\":%s,\"maximized\":%s,\"focused\":%s}",
                                  w->id, w->app_id, w->title, w->x, w->y, w->w, w->h,
                                  w->state == WM_ST_MINIMIZED ? "true" : "false",
                                  w->state == WM_ST_MAXIMIZED ? "true" : "false",
                                  w->focused ? "true" : "false");
            if (o + 64 >= sizeof(msg))
                break;
        }
    }
    o += (size_t)snprintf(msg + o, sizeof(msg) - o, "]}");
    broadcast(wb, msg);
}

static void push_system(WebBridge *wb)
{
    HwProfile hw;
    hwinfo_detect(&hw);
    long mem = wb->gfx && wb->gfx->ram_mb > 0 ? wb->gfx->ram_mb : hw.ram_mb;
    char gfx[192];
    gfx[0] = 0;
    if (wb->gfx) {
        const char *tier = "none";
        if (wb->gfx->gfx_tier == GFX_TIER_GPU)
            tier = "gpu";
        else if (wb->gfx->gfx_tier == GFX_TIER_DRM)
            tier = "drm";
        else if (wb->gfx->gfx_tier == GFX_TIER_FB)
            tier = "fb";
        snprintf(gfx, sizeof(gfx), "tier %s %s %s", tier, wb->gfx->backend[0] ? wb->gfx->backend : "sw",
                 wb->gfx->gpu_name[0] ? wb->gfx->gpu_name : "-");
    }
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char clock[32];
    snprintf(clock, sizeof(clock), "%02d:%02d", tm.tm_hour, tm.tm_min);
    char msg[512];
    snprintf(msg, sizeof(msg),
             "{\"type\":\"system\",\"memoryMb\":%ld,\"gfx\":\"%s\",\"clock\":\"%s\",\"w\":%d,\"h\":%d}",
             mem, gfx, clock, wb->gfx ? wb->gfx->w : 0, wb->gfx ? wb->gfx->h : 0);
    broadcast(wb, msg);
}

static void handle_cmd(WebBridge *wb, const char *json)
{
    char op[64] = {0};
    char id[64] = {0};
    const char *p = strstr(json, "\"op\"");
    if (p) {
        p = strchr(p + 4, '"');
        if (p) {
            p++;
            size_t i = 0;
            while (*p && *p != '"' && i + 1 < sizeof(op))
                op[i++] = *p++;
            op[i] = 0;
        }
    }
    p = strstr(json, "\"id\"");
    if (p) {
        p = strchr(p + 4, '"');
        if (p) {
            p++;
            size_t i = 0;
            while (*p && *p != '"' && i + 1 < sizeof(id))
                id[i++] = *p++;
            id[i] = 0;
        }
    }
    if (str_eq(op, "openApp") && id[0] && wb->wm) {
        PkgRegistry reg;
        pkg_registry_load(&reg);
        if (reg.count == 0)
            pkg_seed_from_dir("/apps", &reg);
        for (int i = 0; i < reg.count; i++) {
            if (str_eq(reg.apps[i].id, id)) {
                wm_open(wb->wm, &reg.apps[i],
                        reg.apps[i].entry_script[0] ? reg.apps[i].entry_script : "bin/main.kbc",
                        reg.apps[i].name);
                push_windows(wb);
                return;
            }
        }
    } else if (str_eq(op, "focus") && wb->wm) {
        wm_focus(wb->wm, atoi(id));
        push_windows(wb);
    } else if (str_eq(op, "minimize") && wb->wm) {
        wm_minimize(wb->wm, atoi(id));
        push_windows(wb);
    } else if (str_eq(op, "maximize") && wb->wm) {
        wm_toggle_max(wb->wm, atoi(id));
        push_windows(wb);
    } else if (str_eq(op, "close") && wb->wm) {
        wm_close(wb->wm, atoi(id));
        push_windows(wb);
    } else if (str_eq(op, "show_desktop") && wb->wm) {
        wm_handle_action(wb->wm, "wm:show_desktop");
        push_windows(wb);
    } else if (str_eq(op, "pkg.refresh")) {
        push_packages(wb);
    } else if (str_eq(op, "system.tick")) {
        push_system(wb);
        if ((wb->tick++ % 3) == 0)
            push_packages(wb);
        push_windows(wb);
    } else if (str_eq(op, "move") || str_eq(op, "resize")) {
        OsWindow *w = wb->wm ? wm_find(wb->wm, atoi(id)) : NULL;
        if (w) {
            const char *px = strstr(json, "\"x\":");
            const char *py = strstr(json, "\"y\":");
            const char *pw = strstr(json, "\"w\":");
            const char *ph = strstr(json, "\"h\":");
            if (px)
                w->x = atoi(px + 4);
            if (py)
                w->y = atoi(py + 4);
            if (pw)
                w->w = atoi(pw + 4);
            if (ph)
                w->h = atoi(ph + 4);
            if (w->state == WM_ST_MAXIMIZED)
                w->state = WM_ST_NORMAL;
        }
    }
}

static void handle_ws_frame(WebBridge *wb, WbClient *c)
{
    if (c->len < 2)
        return;
    unsigned char *b = (unsigned char *)c->buf;
    int masked = (b[1] & 0x80) != 0;
    size_t plen = b[1] & 0x7f;
    size_t off = 2;
    if (plen == 126) {
        if (c->len < 4)
            return;
        plen = ((size_t)b[2] << 8) | b[3];
        off = 4;
    }
    if (!masked)
        return;
    if (c->len < (int)(off + 4 + plen))
        return;
    unsigned char mask[4];
    memcpy(mask, b + off, 4);
    off += 4;
    for (size_t i = 0; i < plen; i++)
        b[off + i] ^= mask[i % 4];
    b[off + plen] = 0;
    const char *payload = (const char *)(b + off);
    if (strstr(payload, "\"type\":\"cmd\"") || strstr(payload, "\"op\""))
        handle_cmd(wb, payload);
    else if (strstr(payload, "subscribe") || strstr(payload, "hello")) {
        push_packages(wb);
        push_windows(wb);
        push_system(wb);
    }
    c->len = 0;
}

static void close_client(WbClient *c)
{
    if (c->fd >= 0)
        close(c->fd);
    c->fd = -1;
    c->is_ws = 0;
    c->handshaked = 0;
    c->len = 0;
}

static void accept_clients(WebBridge *wb)
{
    for (;;) {
        int fd = accept(wb->listen_fd, NULL, NULL);
        if (fd < 0)
            break;
        set_nonblock(fd);
        int slot = -1;
        for (int i = 0; i < WB_MAX_CLIENTS; i++) {
            if (wb->clients[i].fd < 0) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            close(fd);
            continue;
        }
        wb->clients[slot].fd = fd;
        wb->clients[slot].is_ws = 0;
        wb->clients[slot].handshaked = 0;
        wb->clients[slot].len = 0;
    }
}

static void service_client(WebBridge *wb, WbClient *c)
{
    char tmp[4096];
    ssize_t n = read(c->fd, tmp, sizeof(tmp));
    if (n <= 0) {
        close_client(c);
        return;
    }
    if (c->len + (int)n >= WB_BUF) {
        close_client(c);
        return;
    }
    memcpy(c->buf + c->len, tmp, (size_t)n);
    c->len += (int)n;
    c->buf[c->len] = 0;
    if (!c->handshaked) {
        if (!strstr(c->buf, "\r\n\r\n"))
            return;
        if (find_ci(c->buf, "Upgrade: websocket")) {
            char key[128] = {0};
            const char *k = find_ci(c->buf, "Sec-WebSocket-Key:");
            if (k) {
                k += 18;
                while (*k == ' ')
                    k++;
                size_t i = 0;
                while (*k && *k != '\r' && *k != '\n' && i + 1 < sizeof(key))
                    key[i++] = *k++;
                key[i] = 0;
            }
            char accept_key[64];
            ws_accept_key(key, accept_key, sizeof(accept_key));
            char resp[256];
            snprintf(resp, sizeof(resp),
                     "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                     "Sec-WebSocket-Accept: %s\r\n\r\n",
                     accept_key);
            write(c->fd, resp, strlen(resp));
            c->is_ws = 1;
            c->handshaked = 1;
            c->len = 0;
            push_packages(wb);
            push_windows(wb);
            push_system(wb);
            return;
        }
        {
            const char *nf =
                "HTTP/1.1 404 Not Found\r\nContent-Length: 22\r\nConnection: close\r\n\r\nipc websocket only\r\n";
            write(c->fd, nf, strlen(nf));
        }
        close_client(c);
        return;
    }
    if (c->is_ws)
        handle_ws_frame(wb, c);
}

int webbridge_start(WebBridge **out, Gfx *gfx, WindowManager *wm, int port)
{
    if (!out)
        return -1;
    WebBridge *wb = (WebBridge *)calloc(1, sizeof(WebBridge));
    if (!wb)
        return -1;
    wb->gfx = gfx;
    wb->wm = wm;
    wb->port = port > 0 ? port : 9081;
    str_copy(wb->root, sizeof(wb->root), "/ui");
    for (int i = 0; i < WB_MAX_CLIENTS; i++)
        wb->clients[i].fd = -1;
    wb->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (wb->listen_fd < 0) {
        free(wb);
        return -1;
    }
    int yes = 1;
    setsockopt(wb->listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)wb->port);
    if (bind(wb->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        int e1 = errno;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(wb->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            fprintf(stderr, "ipc: bind lo errno=%d any errno=%d\n", e1, errno);
            close(wb->listen_fd);
            free(wb);
            return -1;
        }
        fprintf(stderr, "ipc: bound 0.0.0.0:%d (lo bind errno=%d)\n", wb->port, e1);
    } else {
        fprintf(stderr, "ipc: ws://127.0.0.1:%d/ws\n", wb->port);
    }
    if (listen(wb->listen_fd, 64) != 0) {
        fprintf(stderr, "ipc: listen errno=%d\n", errno);
        close(wb->listen_fd);
        free(wb);
        return -1;
    }
    set_nonblock(wb->listen_fd);
    *out = wb;
    return 0;
}

void webbridge_stop(WebBridge *wb)
{
    if (!wb)
        return;
    for (int i = 0; i < WB_MAX_CLIENTS; i++)
        close_client(&wb->clients[i]);
    if (wb->listen_fd >= 0)
        close(wb->listen_fd);
    free(wb);
}

int webbridge_poll(WebBridge *wb)
{
    if (!wb || wb->listen_fd < 0)
        return -1;
    accept_clients(wb);
    struct pollfd pf[WB_MAX_CLIENTS + 1];
    int n = 0;
    pf[n].fd = wb->listen_fd;
    pf[n].events = POLLIN;
    n++;
    int map[WB_MAX_CLIENTS];
    for (int i = 0; i < WB_MAX_CLIENTS; i++) {
        map[i] = -1;
        if (wb->clients[i].fd < 0)
            continue;
        map[i] = n;
        pf[n].fd = wb->clients[i].fd;
        pf[n].events = POLLIN;
        n++;
    }
    if (poll(pf, (nfds_t)n, 0) > 0) {
        if (pf[0].revents & POLLIN)
            accept_clients(wb);
        for (int i = 0; i < WB_MAX_CLIENTS; i++) {
            if (map[i] < 0)
                continue;
            if (pf[map[i]].revents & (POLLIN | POLLHUP | POLLERR))
                service_client(wb, &wb->clients[i]);
        }
    }
    return 0;
}

int webbridge_running(const WebBridge *wb)
{
    return wb && wb->listen_fd >= 0;
}

int webbridge_port(const WebBridge *wb)
{
    return wb ? wb->port : 0;
}

void webbridge_notify_widgets(WebBridge *wb)
{
    (void)wb;
}

void webbridge_set_root(WebBridge *wb, const char *root)
{
    if (wb && root)
        str_copy(wb->root, sizeof(wb->root), root);
}

#else

struct WebBridge {
    int port;
};

int webbridge_start(WebBridge **out, Gfx *gfx, WindowManager *wm, int port)
{
    (void)gfx;
    (void)wm;
    WebBridge *wb = (WebBridge *)calloc(1, sizeof(WebBridge));
    if (!wb)
        return -1;
    wb->port = port > 0 ? port : 9081;
    *out = wb;
    return -1;
}
void webbridge_stop(WebBridge *wb)
{
    free(wb);
}
int webbridge_poll(WebBridge *wb)
{
    (void)wb;
    return -1;
}
int webbridge_running(const WebBridge *wb)
{
    (void)wb;
    return 0;
}
int webbridge_port(const WebBridge *wb)
{
    return wb ? wb->port : 0;
}
void webbridge_notify_widgets(WebBridge *wb)
{
    (void)wb;
}
void webbridge_set_root(WebBridge *wb, const char *root)
{
    (void)wb;
    (void)root;
}

#endif
