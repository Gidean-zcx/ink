/*
 * Realtime sessions (see ink/sys/rt.h): ids, the frame header, control
 * messages, and routing what arrives to the session's handler. Rides on the
 * hub link: it owns the link's binary receiver and the "rt." notifications.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/hub.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/res.h"
#include "ink/sys/rt.h"

#define FRAME_STACK 336 /* header + a voice or input frame, built on the caller's stack */

static struct {
    osal_mutex_t *lock;
    struct {
        bool used, inflight, ready; /* inflight: its rt.open is on the way; ready: the hub confirmed it */
        int sid;
        ink_rt_handler_t h;
        uint16_t seq[INK_RT_KINDS];
        char member[INK_MEMBER_ID_MAX];
        uint32_t generation, epoch;
        char cap[24];
        char params[INK_RT_PARAMS];
    } s[INK_RT_MAX];
    int next_sid;
} RT INK_PSRAM_BSS;

static int find(int sid) { /* under the lock */
    for (int i = 0; i < INK_RT_MAX; i++)
        if (RT.s[i].used && RT.s[i].sid == sid) return i;
    return -1;
}

static bool handler_of(int sid, ink_rt_handler_t *h) {
    osal_mutex_lock(RT.lock, OSAL_WAIT_FOREVER);
    int i = find(sid);
    if (i >= 0 && (RT.s[i].epoch != ink_hub_member_epoch() || !ink_member_space_open() ||
        strcmp(RT.s[i].member, ink_member_current()->id) || RT.s[i].generation != ink_member_generation(RT.s[i].member))) i = -1;
    if (i >= 0) *h = RT.s[i].h;
    osal_mutex_unlock(RT.lock);
    return i >= 0;
}

static bool drop(int sid) { /* the session ends here: true if it was open */
    osal_mutex_lock(RT.lock, OSAL_WAIT_FOREVER);
    int i = find(sid);
    if (i >= 0) RT.s[i].used = false;
    osal_mutex_unlock(RT.lock);
    if (i >= 0) {
        ink_jobs_hold(false);
        ink_hub_keep(false);
        ink_probe_count("rt.closed");
    }
    return i >= 0;
}

static void on_binary(void *ctx, const uint8_t *d, size_t len) { /* hub task */
    (void)ctx;
    if (len < INK_RT_HDR) {
        ink_probe_count("rt.short_frames");
        return;
    }
    ink_rt_handler_t h;
    if (!handler_of(d[1], &h) || !h.on_frame) {
        ink_probe_count("rt.frames_without_session");
        return;
    }
    uint16_t seq = (uint16_t)(d[2] | d[3] << 8);
    uint32_t t = (uint32_t)d[4] | (uint32_t)d[5] << 8 | (uint32_t)d[6] << 16 | (uint32_t)d[7] << 24;
    h.on_frame(h.ctx, d[1], d[0], d + INK_RT_HDR, len - INK_RT_HDR, seq, t, d[8]);
}

static void on_notify(void *ctx, const char *method, const ink_json_t *j, int params) { /* hub task */
    (void)ctx;
    int64_t sid;
    if (!ink_json_int(j, ink_json_find(j, params, "sid"), &sid)) return;
    ink_rt_handler_t h;
    if (!handler_of((int)sid, &h)) return;
    if (!strcmp(method, "rt.close")) {
        if (drop((int)sid) && h.on_closed) h.on_closed(h.ctx, (int)sid, 0);
        return;
    }
    if (h.on_control) h.on_control(h.ctx, (int)sid, method, j, params);
}

static void open_reply(void *ctx, int status, const ink_json_t *j, int result);

/* Tell the hub about session `sid` (again): on open, and after every reconnect (the hub forgets).
 * < 0: the request could not even be made; nothing will answer it. */
static int announce(int sid) {
    char params[INK_RT_PARAMS + 64];
    osal_mutex_lock(RT.lock, OSAL_WAIT_FOREVER);
    int i = find(sid);
    if (i >= 0) {
        RT.s[i].inflight = true;
        RT.s[i].ready = false;
        snprintf(params, sizeof params, "{\"sid\":%d,\"cap\":\"%s\",\"params\":%s}", sid, RT.s[i].cap, RT.s[i].params);
    }
    osal_mutex_unlock(RT.lock);
    if (i < 0) return 0;
    int r = ink_hub_call("rt.open", params, 10000000, open_reply, (void *)(intptr_t)sid);
    return r < 0 ? r : 0;
}

/* The session cannot go on, and its owner does not know yet. */
static void lost(int sid) {
    ink_rt_handler_t h;
    bool had = handler_of(sid, &h);
    if (drop(sid) && had && h.on_closed) h.on_closed(h.ctx, sid, -1);
    ink_probe_count("rt.refused");
}

static void member_changed(void) {
    for (int i = 0; i < INK_RT_MAX; i++) {
        ink_rt_handler_t h = {0};
        int sid = 0;
        osal_mutex_lock(RT.lock, OSAL_WAIT_FOREVER);
        if (RT.s[i].used && RT.s[i].epoch != ink_hub_member_epoch()) { sid = RT.s[i].sid; h = RT.s[i].h; }
        osal_mutex_unlock(RT.lock);
        if (sid && drop(sid) && h.on_closed) h.on_closed(h.ctx, sid, -1);
    }
}

static void on_online(void *ctx) { /* hub task: a new connection knows no sessions */
    (void)ctx;
    int again[INK_RT_MAX], n = 0;
    osal_mutex_lock(RT.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < INK_RT_MAX; i++)
        if (RT.s[i].used) {
            RT.s[i].ready = false; /* until this connection's hub confirms it */
            if (!RT.s[i].inflight) again[n++] = RT.s[i].sid;
        }
    osal_mutex_unlock(RT.lock);
    for (int k = 0; k < n; k++) {
        ink_probe_count("rt.reannounced");
        if (announce(again[k]) < 0) lost(again[k]);
    }
}

static void open_reply(void *ctx, int status, const ink_json_t *j, int result) { /* hub task */
    (void)j;
    (void)result;
    int sid = (int)(intptr_t)ctx;
    osal_mutex_lock(RT.lock, OSAL_WAIT_FOREVER);
    int i = find(sid);
    if (i >= 0) {
        RT.s[i].inflight = false;
        RT.s[i].ready = status == INK_HUB_OK;
    }
    osal_mutex_unlock(RT.lock);
    ink_rt_handler_t h;
    bool had = handler_of(sid, &h);
    if (status == INK_HUB_OK && had && h.on_ready) h.on_ready(h.ctx, sid);
    if (status == INK_HUB_OK && !had) { /* closed while its rt.open was on the way: the hub has just opened it */
        char params[24];
        snprintf(params, sizeof params, "{\"sid\":%d}", sid);
        ink_hub_notify("rt.close", params);
    }
    if (status == INK_HUB_OK) return;
    if (status == INK_HUB_ERR_LOST || status == INK_HUB_ERR_OFFLINE) return; /* the next connection announces it again */
    if (status == INK_HUB_ERR_TIMEOUT) { /* a hub that does not answer: ask again, the request's own wait paces this */
        if (i >= 0 && ink_hub_online() && announce(sid) < 0) lost(sid);
        return;
    }
    lost(sid); /* the hub refused it, or the request could not be made */
}

int ink_rt_init(void) {
    if (RT.lock) return 0;
    RT.lock = osal_mutex_create();
    if (!RT.lock) return -1;
    ink_hub_on_binary(on_binary, NULL);
    ink_hub_on_member(member_changed);
    if (ink_hub_on_online(on_online, NULL)) return -1;
    return ink_hub_subscribe("rt.", on_notify, NULL);
}

int ink_rt_open(const char *cap, const char *params_json, const ink_rt_handler_t *h) {
    if (!RT.lock || !cap || !h || (params_json && strlen(params_json) >= INK_RT_PARAMS) || strlen(cap) >= 24) return -1;
    osal_mutex_lock(RT.lock, OSAL_WAIT_FOREVER);
    int slot = -1, sid = -1;
    for (int i = 0; i < INK_RT_MAX && slot < 0; i++)
        if (!RT.s[i].used) slot = i;
    if (slot >= 0) {
        do sid = RT.next_sid = RT.next_sid % 255 + 1; /* 1..255, never one in use */
        while (find(sid) >= 0);
        RT.s[slot].used = true;
        RT.s[slot].sid = sid;
        RT.s[slot].h = *h;
        snprintf(RT.s[slot].member, sizeof RT.s[slot].member, "%s", ink_member_current()->id);
        RT.s[slot].generation = ink_member_generation(RT.s[slot].member);
        RT.s[slot].epoch = ink_hub_member_epoch();
        memset(RT.s[slot].seq, 0, sizeof RT.s[slot].seq);
        snprintf(RT.s[slot].cap, sizeof RT.s[slot].cap, "%s", cap);
        snprintf(RT.s[slot].params, sizeof RT.s[slot].params, "%s", params_json && *params_json ? params_json : "{}");
        RT.s[slot].inflight = false;
    }
    osal_mutex_unlock(RT.lock);
    if (sid < 0) {
        ink_probe_count("rt.full");
        return -1;
    }
    ink_jobs_hold(true);
    ink_hub_keep(true); /* connected for as long as the session is open (architecture §9.7) */
    ink_probe_count("rt.opened");
    if (announce(sid) < 0) { /* its owner hears it from the return value */
        drop(sid);
        ink_probe_count("rt.refused");
        return -1;
    }
    return sid;
}

static int class_of(uint8_t kind) {
    return kind == INK_RT_VOICE_UP ? INK_HUB_TX_VOICE : kind == INK_RT_INPUT ? INK_HUB_TX_INPUT : INK_HUB_TX_SCREEN;
}

int ink_rt_send(int sid, uint8_t kind, const void *payload, size_t len, uint8_t flags) {
    if (!RT.lock || kind >= INK_RT_KINDS) return -1;
    osal_mutex_lock(RT.lock, OSAL_WAIT_FOREVER);
    int i = find(sid);
    bool ready = i >= 0 && RT.s[i].ready;
    char member[INK_MEMBER_ID_MAX] = "";
    uint32_t generation = 0, epoch = 0;
    if (ready) {
        snprintf(member, sizeof member, "%s", RT.s[i].member);
        generation = RT.s[i].generation;
        epoch = RT.s[i].epoch;
    }
    uint16_t seq = ready ? RT.s[i].seq[kind]++ : 0;
    osal_mutex_unlock(RT.lock);
    if (!ready) return -1; /* not confirmed by the hub on this connection yet: the caller keeps it */
    uint8_t local[FRAME_STACK];
    uint8_t *f = len + INK_RT_HDR <= sizeof local ? local : osal_malloc(len + INK_RT_HDR, OSAL_MEM_EXTERNAL);
    if (!f) return -1;
    uint32_t t = (uint32_t)(osal_now_us() / 1000u);
    uint8_t hdr[INK_RT_HDR] = {kind, (uint8_t)sid, (uint8_t)seq, (uint8_t)(seq >> 8), (uint8_t)t, (uint8_t)(t >> 8), (uint8_t)(t >> 16), (uint8_t)(t >> 24), flags, 0};
    memcpy(f, hdr, INK_RT_HDR);
    if (len) memcpy(f + INK_RT_HDR, payload, len);
    int r = ink_hub_send_frame_for(f, len + INK_RT_HDR, class_of(kind), member, generation, epoch);
    if (f != local) osal_free(f);
    return r;
}

int ink_rt_control(int sid, const char *method, const char *params_json) {
    char params[256];
    if (params_json && strlen(params_json) + 16 > sizeof params) return -1; /* never cut into bad JSON */
    const char *p = params_json && *params_json == '{' ? params_json + 1 : "}";
    snprintf(params, sizeof params, "{\"sid\":%d%s%s", sid, *p != '}' ? "," : "", p);
    return ink_hub_notify(method, params);
}

void ink_rt_close(int sid) {
    if (!RT.lock || !drop(sid)) return;
    char params[24];
    snprintf(params, sizeof params, "{\"sid\":%d}", sid);
    ink_hub_notify("rt.close", params);
}

bool ink_rt_ready(int sid) {
    if (!RT.lock) return false;
    osal_mutex_lock(RT.lock, OSAL_WAIT_FOREVER);
    int i = find(sid);
    bool r = i >= 0 && RT.s[i].ready;
    osal_mutex_unlock(RT.lock);
    return r && ink_hub_online(); /* ready stays set until the next connection confirms it */
}

bool ink_rt_is_open(int sid) {
    if (!RT.lock) return false;
    osal_mutex_lock(RT.lock, OSAL_WAIT_FOREVER);
    bool r = find(sid) >= 0;
    osal_mutex_unlock(RT.lock);
    return r;
}

int ink_rt_open_count(void) {
    if (!RT.lock) return 0;
    int n = 0;
    osal_mutex_lock(RT.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < INK_RT_MAX; i++) n += RT.s[i].used;
    osal_mutex_unlock(RT.lock);
    return n;
}
