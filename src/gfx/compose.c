#include "compose.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

uint32_t compose_rgb565_to_argb(uint16_t c)
{
    uint32_t r = (c >> 11) & 0x1F;
    uint32_t g = (c >> 5) & 0x3F;
    uint32_t b = c & 0x1F;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

uint16_t compose_argb_to_rgb565(uint32_t c)
{
    uint32_t r = (c >> 16) & 0xFF;
    uint32_t g = (c >> 8) & 0xFF;
    uint32_t b = c & 0xFF;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

uint32_t compose_blend(uint32_t dst, uint32_t src)
{
    uint32_t sa = (src >> 24) & 0xFF;
    if (sa >= 255)
        return src | 0xFF000000u;
    if (sa == 0)
        return dst;
    uint32_t inv = 255 - sa;
    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;
    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;
    uint32_t r = (sr * sa + dr * inv) / 255;
    uint32_t g = (sg * sa + dg * inv) / 255;
    uint32_t b = (sb * sa + db * inv) / 255;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static void dirty_expand(DirtyRect *d, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    int x1 = x + w;
    int y1 = y + h;
    if (!d->valid) {
        d->x0 = x;
        d->y0 = y;
        d->x1 = x1;
        d->y1 = y1;
        d->valid = 1;
        return;
    }
    if (x < d->x0)
        d->x0 = x;
    if (y < d->y0)
        d->y0 = y;
    if (x1 > d->x1)
        d->x1 = x1;
    if (y1 > d->y1)
        d->y1 = y1;
}

static int push_cmd(Composer *c, const ComposeCmd *cmd)
{
    if (!c || !cmd || c->cmd_count >= COMPOSE_MAX_CMDS)
        return -1;
    c->cmds[c->cmd_count++] = *cmd;
    compose_dirty(c, cmd->x, cmd->y, cmd->w > 0 ? cmd->w : 1, cmd->h > 0 ? cmd->h : 1);
    return 0;
}

int compose_init(Composer *c, int w, int h, const HwProfile *hw)
{
    if (!c || w < 64 || h < 64)
        return -1;
    memset(c, 0, sizeof(*c));
    if (hw)
        c->hw = *hw;
    else
        hwinfo_detect(&c->hw);
    if (compose_resize(c, w, h) != 0)
        return -1;
    gfx_pool_init(&c->pool, &c->hw);
    c->fade_alpha = 255;
    return 0;
}

void compose_shutdown(Composer *c)
{
    if (!c)
        return;
    gfx_pool_shutdown(&c->pool);
    free(c->px);
    c->px = NULL;
    c->pixels = 0;
}

int compose_resize(Composer *c, int w, int h)
{
    if (!c || w < 64 || h < 64)
        return -1;
    size_t need = (size_t)w * (size_t)h;
    if (need > c->pixels || !c->px) {
        uint32_t *n = (uint32_t *)realloc(c->px, need * sizeof(uint32_t));
        if (!n)
            return -1;
        c->px = n;
        c->pixels = need;
    }
    c->w = w;
    c->h = h;
    c->stride = w;
    compose_dirty_all(c);
    return 0;
}

void compose_begin(Composer *c)
{
    if (!c)
        return;
    c->cmd_count = 0;
    c->clip_depth = 0;
}

void compose_set_clip(Composer *c, int x, int y, int w, int h)
{
    if (!c)
        return;
    if (c->clip_depth <= 0)
        c->clip_depth = 1;
    Rect *r = &c->clip[0];
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
}

static int clip_active(const Composer *c, Rect *out)
{
    if (!c || c->clip_depth <= 0 || !out)
        return 0;
    *out = c->clip[0];
    return out->w > 0 && out->h > 0;
}

void compose_dirty(Composer *c, int x, int y, int w, int h)
{
    if (!c)
        return;
    dirty_expand(&c->dirty, x, y, w, h);
}

void compose_dirty_all(Composer *c)
{
    if (!c)
        return;
    c->dirty.x0 = 0;
    c->dirty.y0 = 0;
    c->dirty.x1 = c->w;
    c->dirty.y1 = c->h;
    c->dirty.valid = 1;
}

void compose_clear(Composer *c, uint32_t argb)
{
    ComposeCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = CCMD_CLEAR;
    cmd.x = 0;
    cmd.y = 0;
    cmd.w = c ? c->w : 0;
    cmd.h = c ? c->h : 0;
    cmd.c0 = argb;
    push_cmd(c, &cmd);
}

void compose_fill(Composer *c, int x, int y, int w, int h, uint32_t argb)
{
    ComposeCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = CCMD_FILL;
    cmd.x = x;
    cmd.y = y;
    cmd.w = w;
    cmd.h = h;
    cmd.c0 = argb;
    push_cmd(c, &cmd);
}

void compose_round_rect(Composer *c, int x, int y, int w, int h, int r, uint32_t argb, int fill)
{
    ComposeCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = CCMD_ROUND_RECT;
    cmd.x = x;
    cmd.y = y;
    cmd.w = w;
    cmd.h = h;
    cmd.radius = r;
    cmd.fill = fill;
    cmd.c0 = argb;
    push_cmd(c, &cmd);
}

void compose_gradient(Composer *c, int x, int y, int w, int h, uint32_t c0, uint32_t c1, int vertical)
{
    ComposeCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = CCMD_GRADIENT;
    cmd.x = x;
    cmd.y = y;
    cmd.w = w;
    cmd.h = h;
    cmd.c0 = c0;
    cmd.c1 = c1;
    cmd.vertical = vertical;
    push_cmd(c, &cmd);
}

void compose_line_aa(Composer *c, int x0, int y0, int x1, int y1, uint32_t argb)
{
    ComposeCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = CCMD_LINE_AA;
    cmd.x = x0 < x1 ? x0 : x1;
    cmd.y = y0 < y1 ? y0 : y1;
    cmd.w = (x0 > x1 ? x0 - x1 : x1 - x0) + 2;
    cmd.h = (y0 > y1 ? y0 - y1 : y1 - y0) + 2;
    cmd.x1 = x1;
    cmd.y1 = y1;
    cmd.c0 = argb;
    cmd.x = x0;
    cmd.y = y0;
    push_cmd(c, &cmd);
}

void compose_mark_fade(Composer *c, int alpha)
{
    if (c)
        c->fade_alpha = clamp_i(alpha, 0, 255);
}

static void put_px(Composer *c, int x, int y, uint32_t argb)
{
    if (!c || !c->px || x < 0 || y < 0 || x >= c->w || y >= c->h)
        return;
    Rect clip;
    if (clip_active(c, &clip)) {
        if (x < clip.x || y < clip.y || x >= clip.x + clip.w || y >= clip.y + clip.h)
            return;
    }
    uint32_t *p = &c->px[y * c->stride + x];
    *p = compose_blend(*p, argb);
}

static void fill_rect_imm(Composer *c, int x, int y, int w, int h, uint32_t argb)
{
    if (!c || !c->px || w <= 0 || h <= 0)
        return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > c->w)
        w = c->w - x;
    if (y + h > c->h)
        h = c->h - y;
    Rect clip;
    if (clip_active(c, &clip)) {
        int x1 = x + w;
        int y1 = y + h;
        if (x < clip.x)
            x = clip.x;
        if (y < clip.y)
            y = clip.y;
        if (x1 > clip.x + clip.w)
            x1 = clip.x + clip.w;
        if (y1 > clip.y + clip.h)
            y1 = clip.y + clip.h;
        w = x1 - x;
        h = y1 - y;
    }
    if (w <= 0 || h <= 0)
        return;
    uint32_t sa = (argb >> 24) & 0xFF;
    for (int row = y; row < y + h; row++) {
        uint32_t *p = &c->px[row * c->stride + x];
        if (sa >= 255) {
            for (int col = 0; col < w; col++)
                p[col] = argb | 0xFF000000u;
        } else {
            for (int col = 0; col < w; col++)
                p[col] = compose_blend(p[col], argb);
        }
    }
}

static float cover_circle(float px, float py, float cx, float cy, float r)
{
    float dx = px - cx;
    float dy = py - cy;
    float d = sqrtf(dx * dx + dy * dy);
    float dist = r - d;
    if (dist >= 1.0f)
        return 1.0f;
    if (dist <= -1.0f)
        return 0.0f;
    return (dist + 1.0f) * 0.5f;
}

static void corner_cov(Composer *c, int x, int y, float cx, float cy, float fr, float inner_r, uint32_t argb, int fill)
{
    uint32_t base_a = (argb >> 24) & 0xFF;
    float cov = cover_circle((float)x + 0.5f, (float)y + 0.5f, cx, cy, fr);
    if (!fill) {
        float inner = cover_circle((float)x + 0.5f, (float)y + 0.5f, cx, cy, inner_r);
        cov = cov - inner;
        if (cov < 0.0f)
            cov = 0.0f;
    }
    if (cov <= 0.01f)
        return;
    uint32_t a = (uint32_t)((float)base_a * cov);
    if (a > 255)
        a = 255;
    put_px(c, x, y, (argb & 0x00FFFFFFu) | (a << 24));
}

static void round_rect_imm(Composer *c, int x, int y, int w, int h, int r, uint32_t argb, int fill)
{
    if (!c || w <= 0 || h <= 0)
        return;
    if (r < 0)
        r = 0;
    if (r * 2 > w)
        r = w / 2;
    if (r * 2 > h)
        r = h / 2;
    if (r <= 0) {
        if (fill)
            fill_rect_imm(c, x, y, w, h, argb);
        else {
            fill_rect_imm(c, x, y, w, 1, argb);
            fill_rect_imm(c, x, y + h - 1, w, 1, argb);
            fill_rect_imm(c, x, y, 1, h, argb);
            fill_rect_imm(c, x + w - 1, y, 1, h, argb);
        }
        return;
    }
    float fr = (float)r;
    float inner_r = fr - 1.15f;
    if (inner_r < 0.5f)
        inner_r = 0.5f;
    if (fill) {
        fill_rect_imm(c, x + r, y, w - 2 * r, h, argb);
        fill_rect_imm(c, x, y + r, r, h - 2 * r, argb);
        fill_rect_imm(c, x + w - r, y + r, r, h - 2 * r, argb);
    } else {
        fill_rect_imm(c, x + r, y, w - 2 * r, 1, argb);
        fill_rect_imm(c, x + r, y + h - 1, w - 2 * r, 1, argb);
        fill_rect_imm(c, x, y + r, 1, h - 2 * r, argb);
        fill_rect_imm(c, x + w - 1, y + r, 1, h - 2 * r, argb);
    }
    float ctlx = (float)(x + r);
    float ctly = (float)(y + r);
    float ctrx = (float)(x + w - r);
    float cbry = (float)(y + h - r);
    for (int j = 0; j < r; j++) {
        for (int i = 0; i < r; i++) {
            corner_cov(c, x + i, y + j, ctlx, ctly, fr, inner_r, argb, fill);
            corner_cov(c, x + w - 1 - i, y + j, ctrx - 1.0f, ctly, fr, inner_r, argb, fill);
            corner_cov(c, x + i, y + h - 1 - j, ctlx, cbry - 1.0f, fr, inner_r, argb, fill);
            corner_cov(c, x + w - 1 - i, y + h - 1 - j, ctrx - 1.0f, cbry - 1.0f, fr, inner_r, argb, fill);
        }
    }
}

static void gradient_imm(Composer *c, int x, int y, int w, int h, uint32_t c0, uint32_t c1, int vertical)
{
    if (!c || w <= 0 || h <= 0)
        return;
    uint32_t r0 = (c0 >> 16) & 0xFF, g0 = (c0 >> 8) & 0xFF, b0 = c0 & 0xFF, a0 = (c0 >> 24) & 0xFF;
    uint32_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF, a1 = (c1 >> 24) & 0xFF;
    int steps = vertical ? h : w;
    if (steps < 1)
        steps = 1;
    for (int i = 0; i < steps; i++) {
        uint32_t t = (uint32_t)i;
        uint32_t r = (r0 * (steps - 1 - (int)t) + r1 * (int)t) / (steps - 1 ? steps - 1 : 1);
        uint32_t g = (g0 * (steps - 1 - (int)t) + g1 * (int)t) / (steps - 1 ? steps - 1 : 1);
        uint32_t b = (b0 * (steps - 1 - (int)t) + b1 * (int)t) / (steps - 1 ? steps - 1 : 1);
        uint32_t a = (a0 * (steps - 1 - (int)t) + a1 * (int)t) / (steps - 1 ? steps - 1 : 1);
        uint32_t col = (a << 24) | (r << 16) | (g << 8) | b;
        if (vertical)
            fill_rect_imm(c, x, y + i, w, 1, col);
        else
            fill_rect_imm(c, x + i, y, 1, h, col);
    }
}

static void line_aa_imm(Composer *c, int x0, int y0, int x1, int y1, uint32_t argb)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int steps = adx > ady ? adx : ady;
    if (steps < 1)
        steps = 1;
    uint32_t base_a = (argb >> 24) & 0xFF;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float fx = (float)x0 + (float)dx * t;
        float fy = (float)y0 + (float)dy * t;
        int ix = (int)floorf(fx);
        int iy = (int)floorf(fy);
        float fxr = fx - (float)ix;
        float fyr = fy - (float)iy;
        float w00 = (1.0f - fxr) * (1.0f - fyr);
        float w10 = fxr * (1.0f - fyr);
        float w01 = (1.0f - fxr) * fyr;
        float w11 = fxr * fyr;
        put_px(c, ix, iy, (argb & 0x00FFFFFFu) | ((uint32_t)(base_a * w00) << 24));
        put_px(c, ix + 1, iy, (argb & 0x00FFFFFFu) | ((uint32_t)(base_a * w10) << 24));
        put_px(c, ix, iy + 1, (argb & 0x00FFFFFFu) | ((uint32_t)(base_a * w01) << 24));
        put_px(c, ix + 1, iy + 1, (argb & 0x00FFFFFFu) | ((uint32_t)(base_a * w11) << 24));
    }
}

static void exec_cmd(Composer *c, const ComposeCmd *cmd)
{
    switch (cmd->type) {
    case CCMD_CLEAR:
        fill_rect_imm(c, 0, 0, c->w, c->h, cmd->c0);
        break;
    case CCMD_FILL:
        fill_rect_imm(c, cmd->x, cmd->y, cmd->w, cmd->h, cmd->c0);
        break;
    case CCMD_ROUND_RECT:
        round_rect_imm(c, cmd->x, cmd->y, cmd->w, cmd->h, cmd->radius, cmd->c0, cmd->fill);
        break;
    case CCMD_GRADIENT:
        gradient_imm(c, cmd->x, cmd->y, cmd->w, cmd->h, cmd->c0, cmd->c1, cmd->vertical);
        break;
    case CCMD_LINE_AA:
        line_aa_imm(c, cmd->x, cmd->y, cmd->x1, cmd->y1, cmd->c0);
        break;
    default:
        break;
    }
}

typedef struct {
    Composer *c;
    const ComposeCmd *cmds;
    int count;
} FlushJob;

static void flush_band(int y0, int y1, void *user)
{
    FlushJob *job = (FlushJob *)user;
    Composer *c = job->c;
    for (int i = 0; i < job->count; i++) {
        const ComposeCmd *cmd = &job->cmds[i];
        if (cmd->type == CCMD_CLEAR) {
            fill_rect_imm(c, 0, y0, c->w, y1 - y0, cmd->c0);
            continue;
        }
        int cy1 = cmd->y + (cmd->h > 0 ? cmd->h : 1);
        if (cmd->type == CCMD_LINE_AA) {
            int miny = cmd->y < cmd->y1 ? cmd->y : cmd->y1;
            int maxy = cmd->y > cmd->y1 ? cmd->y : cmd->y1;
            if (maxy < y0 || miny >= y1)
                continue;
            exec_cmd(c, cmd);
            continue;
        }
        if (cy1 < y0 || cmd->y >= y1)
            continue;
        exec_cmd(c, cmd);
    }
}

void compose_flush(Composer *c, uint16_t *dst565, int dst_w, int dst_h)
{
    if (!c || !c->px)
        return;
    int x0 = 0, y0 = 0, x1 = c->w, y1 = c->h;
    if (c->dirty.valid) {
        x0 = clamp_i(c->dirty.x0, 0, c->w);
        y0 = clamp_i(c->dirty.y0, 0, c->h);
        x1 = clamp_i(c->dirty.x1, 0, c->w);
        y1 = clamp_i(c->dirty.y1, 0, c->h);
    }
    if (c->cmd_count > 0) {
        FlushJob job;
        job.c = c;
        job.cmds = c->cmds;
        job.count = c->cmd_count;
        gfx_pool_parallel_bands(&c->pool, y0, y1, flush_band, &job);
        c->cmd_count = 0;
    }
    if (dst565) {
        int cw = c->w < dst_w ? c->w : dst_w;
        int ch = c->h < dst_h ? c->h : dst_h;
        if (x1 > cw)
            x1 = cw;
        if (y1 > ch)
            y1 = ch;
        for (int y = y0; y < y1; y++) {
            uint32_t *src = &c->px[y * c->stride];
            uint16_t *dst = &dst565[y * dst_w];
            for (int x = x0; x < x1; x++)
                dst[x] = compose_argb_to_rgb565(src[x]);
        }
    }
    c->dirty.valid = 0;
}
