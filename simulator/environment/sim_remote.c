/*
 * The hub's rendering capability "browser.session", prototyped for the
 * simulator (detailed design §8.3). No real browser: a few generated pages
 * (a title bar, two links under it, lines of word-like marks, one photograph). What it
 * does like the real one: it renders at the device's size, compares with what
 * the device shows, sends only the changed areas (16-pixel bands; text and
 * interface as 4-bit gray + LZ4, the photograph as JPEG, the simulator's
 * stand-in), one update per credit the device returned (it pulls), keeps only
 * the newest content while it has no credit, and follows taps on links after
 * a render time. Runs in event (ISR) context, like sim_hub.c.
 */
#include <stdlib.h>
#include <string.h>

#include "ink/sys/lz4.h"
#include "sim.h"

#define BAND 16
#define RENDER_FIRST_MS 200 /* first load: the page is fetched and laid out */
#define RENDER_MS 80        /* after a tap */
#define PAGES 3

typedef struct {
    bool used;
    struct hal_ws *ws;
    int sid;
    int w, h;
    int page;
    uint8_t *shown;   /* what the device has (8-bit gray) */
    uint8_t *now;     /* the current render */
    int credits;
    bool dirty;       /* now != shown */
    uint16_t seq;
    unsigned updates;
} rs_t;

static rs_t RS[2];

static void fill(uint8_t *px, int w, int h, int x, int y, int fw, int fh, uint8_t v) {
    for (int r = y; r < y + fh && r < h; r++)
        for (int c = x; c < x + fw && c < w; c++)
            if (r >= 0 && c >= 0) px[(size_t)r * (size_t)w + (size_t)c] = v;
}

static void photo_rect(const rs_t *s, int *x, int *y, int *w, int *h) {
    *x = 24;
    *y = s->h / 2 - 40;
    *w = s->w - 48;
    *h = 200;
}

static void link_rect(const rs_t *s, int i, int *x, int *y, int *w, int *h) { /* under the title bar */
    *w = 200;
    *h = 50;
    *x = i ? s->w - 24 - 200 : 24;
    *y = 60;
}

static void render(rs_t *s) {
    uint8_t *px = s->now;
    int w = s->w, h = s->h;
    memset(px, 0xFF, (size_t)w * (size_t)h);
    fill(px, w, h, 0, 0, w, 48, 0x22); /* title bar */
    fill(px, w, h, 24, 14, 120 + 40 * s->page, 20, 0xEE);
    uint32_t seed = 7919u * (uint32_t)(s->page + 1);
    int px_, py, pw, ph;
    photo_rect(s, &px_, &py, &pw, &ph);
    for (int y = 130; y + 14 < h - 40; y += 28) { /* lines of words, skipping the photograph */
        if (y + 14 > py && y < py + ph) continue;
        for (int x = 24; x < w - 24;) {
            seed = seed * 1103515245u + 12345u;
            int len = 20 + (int)(seed >> 26);
            if (x + len > w - 24) break;
            fill(px, w, h, x, y, len, 14, 0x33);
            x += len + 10;
        }
    }
    for (int y = 0; y < ph; y++) /* the photograph: a gradient with grain */
        for (int x = 0; x < pw; x++) {
            seed = seed * 1103515245u + 12345u;
            int v = 40 + (x * 160) / pw + (y * 40) / ph + (int)(seed >> 28) - 8 + s->page * 10;
            px[(size_t)(py + y) * (size_t)w + (size_t)(px_ + x)] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
    for (int i = 0; i < 2; i++) { /* links: previous, next */
        int lx, ly, lw, lh;
        link_rect(s, i, &lx, &ly, &lw, &lh);
        fill(px, w, h, lx, ly, lw, lh, 0x11);
        fill(px, w, h, lx + 4, ly + 4, lw - 8, lh - 8, 0xDD);
    }
    s->dirty = true;
}

/* One region frame: [x][y][w][h] u16, encoding, refresh hint, data. */
static size_t region(rs_t *s, uint8_t *out, size_t cap, int x, int y, int w, int h, bool photo, uint8_t hint) {
    uint8_t *p = out + 10;
    p[0] = (uint8_t)x, p[1] = (uint8_t)(x >> 8), p[2] = (uint8_t)y, p[3] = (uint8_t)(y >> 8);
    p[4] = (uint8_t)w, p[5] = (uint8_t)(w >> 8), p[6] = (uint8_t)h, p[7] = (uint8_t)(h >> 8);
    p[8] = photo ? 2 : 1;
    p[9] = hint;
    uint8_t *d = p + 10;
    size_t room = cap - 20, n;
    if (photo) { /* the JPEG stand-in: half resolution, 8-bit */
        int hw = (w + 1) / 2, hh = (h + 1) / 2;
        d[0] = 'S', d[1] = 'J', d[2] = (uint8_t)w, d[3] = (uint8_t)(w >> 8), d[4] = (uint8_t)h, d[5] = (uint8_t)(h >> 8);
        for (int r = 0; r < hh; r++)
            for (int c = 0; c < hw; c++) d[6 + (size_t)r * (size_t)hw + (size_t)c] = s->now[(size_t)(y + 2 * r) * (size_t)s->w + (size_t)(x + 2 * c)];
        n = 6 + (size_t)hw * (size_t)hh;
    } else { /* 16 gray levels, two pixels a byte, then LZ4 */
        size_t px = (size_t)w * (size_t)h, packed = (px + 1) / 2;
        uint8_t *nib = sim_host_malloc(packed);
        if (!nib) abort();
        memset(nib, 0, packed);
        for (size_t i = 0; i < px; i++) {
            int r = (int)(i / (size_t)w), c = (int)(i % (size_t)w);
            uint8_t v = (uint8_t)((s->now[(size_t)(y + r) * (size_t)s->w + (size_t)(x + c)] + 8) / 17);
            nib[i / 2] |= (uint8_t)(i & 1 ? v : v << 4);
        }
        static uint32_t work[INK_LZ4_WORK / 4];
        int k = ink_lz4_encode(nib, packed, d, room, work);
        sim_host_free(nib);
        if (k < 0) abort();
        n = (size_t)k;
    }
    return 10 + n;
}

static void send_frame(rs_t *s, uint64_t t, uint8_t *f, size_t len, bool last) {
    uint32_t t_ms = (uint32_t)(t / 1000000u);
    f[0] = 3, f[1] = (uint8_t)s->sid, f[2] = (uint8_t)s->seq, f[3] = (uint8_t)(s->seq >> 8);
    f[4] = (uint8_t)t_ms, f[5] = (uint8_t)(t_ms >> 8), f[6] = (uint8_t)(t_ms >> 16), f[7] = (uint8_t)(t_ms >> 24);
    f[8] = last ? 1 : 0, f[9] = 0;
    s->seq++;
    sim_net_hub_send_at(s->ws, t, f, 10 + len, true);
    sim_metric_add("hub.remote.bytes", (double)(10 + len));
}

/* Send what changed since the device's last picture, as one update (one credit). */
static void push(rs_t *s, uint64_t t) {
    if (!s->dirty || s->credits <= 0) {
        if (s->dirty) sim_metric_add("hub.remote.waiting_for_credit", 1);
        return;
    }
    int bands = (s->h + BAND - 1) / BAND, px_, py, pw, ph;
    photo_rect(s, &px_, &py, &pw, &ph);
    uint8_t hint = s->updates ? 1 : 2; /* the first picture: a clean full refresh */
    size_t cap = (size_t)s->w * BAND + 64;
    uint8_t *f = sim_host_malloc(cap + 32);
    if (!f) abort();
    int last_y = -1;
    for (int b = 0; b < bands; b++) { /* find the last changed band first: it carries the end flag */
        int y = b * BAND, h = y + BAND > s->h ? s->h - y : BAND;
        for (int r = y; r < y + h && last_y < y; r++)
            if (memcmp(s->now + (size_t)r * (size_t)s->w, s->shown + (size_t)r * (size_t)s->w, (size_t)s->w)) last_y = y;
    }
    size_t total = 0;
    for (int b = 0; b < bands && last_y >= 0; b++) {
        int y = b * BAND, h = y + BAND > s->h ? s->h - y : BAND, x0 = s->w, x1 = -1;
        for (int r = y; r < y + h; r++)
            for (int c = 0; c < s->w; c++)
                if (s->now[(size_t)r * (size_t)s->w + (size_t)c] != s->shown[(size_t)r * (size_t)s->w + (size_t)c]) {
                    if (c < x0) x0 = c;
                    if (c > x1) x1 = c;
                }
        if (x1 < 0) continue;
        bool photo = y >= py && y + h <= py + ph && x0 >= px_ && x1 < px_ + pw;
        if (y >= py && y < py + ph) { /* in the photograph's rows: the photograph's columns go as JPEG */
            x0 = px_;
            x1 = px_ + pw - 1;
            photo = true;
        }
        size_t n = region(s, f, cap, x0, y, x1 - x0 + 1, h, photo, hint);
        send_frame(s, t, f, n, y == last_y);
        total += n + 10;
        for (int r = y; r < y + h; r++) memcpy(s->shown + (size_t)r * (size_t)s->w + x0, s->now + (size_t)r * (size_t)s->w + x0, (size_t)(x1 - x0 + 1));
    }
    sim_host_free(f);
    s->dirty = false;
    s->credits--;
    s->updates++;
    sim_metric_add("hub.remote.updates", 1);
    sim_metric_sample("hub.remote.update_bytes", (double)total);
}

typedef struct {
    rs_t *s;
    struct hal_ws *ws; /* the connection that asked: nothing for a later one */
    int page;
} nav_t;

static void navigated(void *arg) { /* ISR: the page is rendered */
    nav_t *n = arg;
    if (n->s->used && n->s->ws == n->ws) {
        n->s->page = n->page;
        render(n->s);
        push(n->s, vtk_now());
    }
    sim_host_free(n);
}

static void navigate(rs_t *s, uint64_t t, int page, int render_ms) {
    nav_t *n = sim_host_malloc(sizeof *n);
    if (!n) abort();
    n->s = s;
    n->ws = s->ws;
    n->page = page;
    vtk_event_at(t + SIM_MS(render_ms), navigated, n);
}

static rs_t *find(struct hal_ws *ws, int sid) {
    for (int i = 0; i < 2; i++)
        if (RS[i].used && RS[i].ws == ws && RS[i].sid == sid) return &RS[i];
    return NULL;
}

bool sim_remote_open(struct hal_ws *ws, uint64_t t, int sid, int w, int h) {
    if (find(ws, sid)) return true;
    for (int i = 0; i < 2; i++)
        if (RS[i].used && RS[i].ws != ws) { /* a new connection: the hub has forgotten the old one's sessions */
            sim_host_free(RS[i].shown);
            sim_host_free(RS[i].now);
            RS[i].used = false;
        }
    for (int i = 0; i < 2; i++)
        if (!RS[i].used) {
            rs_t *s = &RS[i];
            *s = (rs_t){.used = true, .ws = ws, .sid = sid, .w = w > 0 && w <= 2048 ? w : 684, .h = h > 0 && h <= 2048 ? h : 1216};
            s->shown = sim_host_malloc((size_t)s->w * (size_t)s->h);
            s->now = sim_host_malloc((size_t)s->w * (size_t)s->h);
            if (!s->shown || !s->now) abort();
            memset(s->shown, 0xFF, (size_t)s->w * (size_t)s->h); /* the device starts white */
            navigate(s, t, 0, RENDER_FIRST_MS);
            sim_metric_add("hub.remote.sessions", 1);
            return true;
        }
    return false;
}

const uint8_t *sim_remote_shown(int sid, int *w, int *h) {
    for (int i = 0; i < 2; i++)
        if (RS[i].used && RS[i].sid == sid) {
            *w = RS[i].w;
            *h = RS[i].h;
            return RS[i].shown;
        }
    return NULL;
}

void sim_remote_close(struct hal_ws *ws, int sid) {
    rs_t *s = find(ws, sid);
    if (!s) return;
    sim_host_free(s->shown);
    sim_host_free(s->now);
    s->used = false;
}

bool sim_remote_frame(struct hal_ws *ws, uint64_t t, const uint8_t *d, size_t len) {
    if (len < 10) return false;
    rs_t *s = find(ws, d[1]);
    if (!s) return false;
    if (d[0] == 5 && len >= 11) { /* credit */
        s->credits += d[10];
        push(s, t);
    } else if (d[0] == 4 && len >= 15 && d[10] == 1) { /* tap */
        int x = d[11] | d[12] << 8, y = d[13] | d[14] << 8;
        sim_metric_add("hub.remote.taps", 1);
        for (int i = 0; i < 2; i++) {
            int lx, ly, lw, lh;
            link_rect(s, i, &lx, &ly, &lw, &lh);
            if (x >= lx && x < lx + lw && y >= ly && y < ly + lh) {
                sim_metric_add("hub.remote.link_hits", 1);
                navigate(s, t, (s->page + (i ? 1 : PAGES - 1)) % PAGES, RENDER_MS);
            }
        }
    }
    return true;
}
