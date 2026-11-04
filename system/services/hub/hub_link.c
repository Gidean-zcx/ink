/*
 * Hub link service task.
 *
 *  DISCONNECTED --demand/call--> CONNECTING --ws connected--> HELLO --hello ok--> ONLINE
 *        ^                              |                         |                   |
 *        +---- idle 60 s / closed <-----+---- failure: backoff 1,2,4..60 s (+jitter) <+
 *
 * Requests made while not online wait in the outbox until their timeout;
 * requests in flight when the connection drops fail with INK_HUB_ERR_LOST
 * (callers decide whether to retry; durable sync uses the storage service).
 *
 * Threads: the hub task owns the connection and everything about it. Other
 * tasks only send it messages, read the state (atomic), set the demand
 * deadline and queue fire-and-forget messages (both under tx_lock, which
 * also reserves the send budget before the message is queued).
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/hal_net.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/hub.h"
#include "ink/sys/power.h"

#define HUB_STACK 6144 /* PSRAM: replies run here, and what they start (closing a session, the next request) */
#define HUB_PRIO 6
#define HUB_QUEUE 16
#define MAX_PENDING 16
#define MAX_SUBS 8
#define IDLE_DISCONNECT_US (60ull * 1000000ull)
#define DEMAND_HOLD_US (30ull * 1000000ull)
#define HELLO_TIMEOUT_US (10ull * 1000000ull)

typedef enum { HS_DISCONNECTED, HS_CONNECTING, HS_HELLO, HS_ONLINE, HS_BACKOFF } hub_state_t;
typedef enum { HE_CALL = 1, HE_WS, HE_LINK, HE_DEMAND, HE_TX } hub_ev_t;

/* Fire-and-forget messages (notifications, binary frames): FIFOs in PSRAM, one per class, drained
 * by the hub task while online, highest class first (voice > input > screen > control >
 * background, detailed design §6.4), so callers never touch the WebSocket handle owned by the hub
 * task. Small binary frames (voice, input) come from a pool allocated once: no allocation per frame. */
typedef struct tx_item {
    struct tx_item *next;
    bool binary, pooled;
    size_t len;
    uint8_t data[];
} tx_item_t;
#define TX_MAX_BYTES (96 * 1024)
#define TX_CLASSES 5
#define TXP_N 48         /* pooled items: ~3 s of 60 ms voice frames queued */
#define TXP_PAYLOAD 320  /* a frame header and an Opus frame */
#define BG_GAP_US 20000  /* while a realtime session is open: one background message per gap, so the
                          * platform's (first-in, first-out) send buffer never holds much ahead of voice */

typedef struct {
    uint32_t id;
    char method[32];
    char *params;
    int64_t timeout_us;
    ink_hub_reply_fn cb;
    void *ctx;
} call_req_t; /* lives in PSRAM; only its pointer travels through the (internal-RAM) queue */

typedef struct {
    uint8_t type;
    uint8_t ws_event;
    uint32_t ws_gen; /* the connection an HE_WS event came from */
    call_req_t *req;
    char *data; /* heap copy (PSRAM) */
    size_t len;
} hub_msg_t;

typedef struct {
    uint32_t id;
    char method[32];
    char *params;
    uint64_t deadline;
    uint64_t sent_at;
    ink_hub_reply_fn cb;
    void *ctx;
    bool sent, used;
} pending_t;

static struct {
    char url[128];
    osal_queue_t *q;
    osal_task_t *task;
    osal_mutex_t *id_lock;
    hal_ws_t *ws;
    hal_link_type_t ws_link; /* hub task: the link it was made on */
    uint32_t ws_gen; /* hub task: the current connection's number (its callback's ctx) */
    hub_state_t state; /* written by the hub task only (set_state), read by any task (atomic) */
    pending_t pend[MAX_PENDING];
    uint32_t next_id;
    uint32_t hello_id;
    uint64_t hello_sent_us;
    int64_t server_ms;     /* the hub's clock when this connection came up, 0 = it did not say */
    uint32_t hello_rtt_ms;
    uint64_t last_activity, backoff_until, backoff_us, connect_started; /* hub task */
    uint64_t demand_until; /* under tx_lock: 64 bits are not one access on RV32 */
    bool low_power;        /* atomic: device locked, disconnect as soon as idle */
    struct {
        char prefix[16];
        ink_hub_notify_fn fn;
        void *ctx;
    } subs[MAX_SUBS];
    int nsubs;
    ink_json_t *json; /* parse buffer (PSRAM, ~2 KB): keeps the token array off the stack */
    void (*tick)(void);
    osal_mutex_t *tx_lock; /* the tx queue, its byte count and demand_until */
    tx_item_t *tx_head[TX_CLASSES], *tx_tail[TX_CLASSES];
    tx_item_t *tx_free; /* the pool's unused items */
    int keep;           /* realtime sessions open: stay connected (atomic) */
    uint64_t bg_next;   /* hub task: earliest time for the next background message while keep > 0 */
    void (*online_fn[4])(void *ctx);
    void *online_ctx[4];
    int nonline;
    size_t tx_bytes;    /* reserved: queued plus being queued */
    ink_hub_binary_fn bin_fn;
    void *bin_ctx;
    struct {
        char prefix[16];
        ink_hub_request_fn fn;
        void *ctx;
    } reqs[4];
    int nreqs;
} H INK_PSRAM_BSS;

static void set_state(hub_state_t s) { __atomic_store_n(&H.state, s, __ATOMIC_RELEASE); }
static hub_state_t get_state(void) { return __atomic_load_n(&H.state, __ATOMIC_ACQUIRE); }
static bool low_power(void) { return __atomic_load_n(&H.low_power, __ATOMIC_ACQUIRE); }

static uint64_t demand_until(void) {
    osal_mutex_lock(H.tx_lock, OSAL_WAIT_FOREVER);
    uint64_t t = H.demand_until;
    osal_mutex_unlock(H.tx_lock);
    return t;
}

static char *dup_mem(const void *p, size_t n) {
    char *c = osal_malloc(n + 1, OSAL_MEM_EXTERNAL);
    if (!c) return NULL;
    memcpy(c, p, n);
    c[n] = 0;
    return c;
}

/* ------------------------------------------------------------ HAL glue */

#define WS_POST_WAIT_US 500000

static void ws_cb(void *ctx, hal_ws_event_t ev, const uint8_t *data, size_t len) { /* platform network task */
    hub_msg_t m = {.type = HE_WS, .ws_event = (uint8_t)ev, .ws_gen = (uint32_t)(uintptr_t)ctx};
    if (data && len) {
        const char *prev = osal_mem_tag("hub");
        m.data = dup_mem(data, len);
        osal_mem_tag(prev);
        m.len = len;
    }
    /* bounded (hal_net.h): the hub task may be waiting in hal_ws_free for this callback, and then it
     * is discarding this connection anyway */
    if (osal_queue_send(H.q, &m, WS_POST_WAIT_US) != OSAL_OK) {
        osal_free(m.data);
        ink_probe_count("hub.rx_dropped");
    }
}

static void link_cb(void *ctx, const hal_link_status_t *st) {
    (void)ctx;
    (void)st;
    hub_msg_t m = {.type = HE_LINK};
    osal_queue_send(H.q, &m, 0);
}

/* ------------------------------------------------------------ helpers */

static void finish(pending_t *p, int status, const ink_json_t *msg, int tok) {
    if (!p->used) return;
    p->used = false;
    char *params = p->params; /* the callback may make a new request, in this slot */
    p->params = NULL;
    ink_hub_reply_fn cb = p->cb;
    void *ctx = p->ctx;
    if (p->sent_at && status == INK_HUB_OK) ink_probe_value("hub.request_ms", (double)(osal_now_us() - p->sent_at) / 1000.0);
    if (status != INK_HUB_OK) {
        char key[48];
        snprintf(key, sizeof key, "hub.errors.%d", -status);
        ink_probe_count(key);
    }
    if (cb) cb(ctx, status, msg, tok);
    osal_free(params);
}

static int send_request(pending_t *p) {
    char head[96];
    size_t plen = strlen(p->params ? p->params : "{}");
    snprintf(head, sizeof head, "{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"%s\",\"params\":", p->id, p->method);
    size_t hlen = strlen(head), total = hlen + plen + 1;
    const char *prev = osal_mem_tag("hub");
    char *buf = osal_malloc(total + 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!buf) return -1;
    memcpy(buf, head, hlen);
    memcpy(buf + hlen, p->params ? p->params : "{}", plen);
    buf[hlen + plen] = '}';
    buf[total] = 0;
    int r = hal_ws_send(H.ws, false, buf, total, 2000000);
    osal_free(buf);
    if (r == 0) {
        p->sent = true;
        p->sent_at = osal_now_us();
        H.last_activity = p->sent_at;
    }
    return r;
}

static uint32_t new_id(void) {
    osal_mutex_lock(H.id_lock, OSAL_WAIT_FOREVER);
    uint32_t id = ++H.next_id;
    osal_mutex_unlock(H.id_lock);
    return id;
}

static pending_t *slot_new(void) {
    for (int i = 0; i < MAX_PENDING; i++)
        if (!H.pend[i].used) return &H.pend[i];
    return NULL;
}

static void close_ws(void) {
    if (H.ws) hal_ws_free(H.ws);
    H.ws = NULL;
}

static void fail_all(int status, bool only_sent) {
    for (int i = 0; i < MAX_PENDING; i++)
        if (H.pend[i].used && (!only_sent || H.pend[i].sent)) finish(&H.pend[i], status, NULL, -1);
}

static void enter_backoff(void) {
    close_ws();
    H.backoff_us = H.backoff_us ? (H.backoff_us * 2 > 60000000ull ? 60000000ull : H.backoff_us * 2) : 1000000ull;
    uint64_t jitter = (osal_now_us() * 2654435761u) % (H.backoff_us / 4 + 1);
    H.backoff_until = osal_now_us() + H.backoff_us + jitter;
    set_state(HS_BACKOFF);
    ink_probe_count("hub.backoffs");
}

static bool has_demand(uint64_t now) {
    if (now < demand_until() || __atomic_load_n(&H.keep, __ATOMIC_ACQUIRE) > 0) return true;
    for (int i = 0; i < MAX_PENDING; i++)
        if (H.pend[i].used) return true;
    return false;
}

static void start_connect(void) {
    hal_link_status_t st;
    hal_net_link_status(&st);
    H.connect_started = osal_now_us();
    H.ws_link = st.type;
    H.ws = hal_ws_open(H.url, ws_cb, (void *)(uintptr_t)++H.ws_gen);
    set_state(H.ws ? HS_CONNECTING : HS_BACKOFF);
    if (!H.ws) enter_backoff();
}

int ink_hub_respond_error(int64_t id, int code, const char *message);

static void on_text(const char *txt, size_t len) {
    ink_json_t *j = H.json;
    if (ink_json_parse(j, txt, len) != 0) {
        ink_probe_count("hub.bad_messages");
        return;
    }
    H.last_activity = osal_now_us();
    int id_t = ink_json_find(j, 0, "id");
    int res_t = ink_json_find(j, 0, "result"), err_t = ink_json_find(j, 0, "error");
    if (id_t >= 0 && (res_t >= 0 || err_t >= 0)) {
        int64_t id;
        if (!ink_json_int(j, id_t, &id)) return;
        if ((uint32_t)id == H.hello_id && H.state == HS_HELLO) {
            if (res_t >= 0) {
                int64_t ms;
                H.server_ms = ink_json_int(j, ink_json_find(j, res_t, "now_ms"), &ms) ? ms : 0;
                H.hello_rtt_ms = (uint32_t)((osal_now_us() - H.hello_sent_us) / 1000u);
                set_state(HS_ONLINE);
                H.backoff_us = 0;
                ink_probe_value("hub.online_ms", (double)(osal_now_us() - H.connect_started) / 1000.0);
                for (int i = 0; i < MAX_PENDING; i++)
                    if (H.pend[i].used && !H.pend[i].sent) send_request(&H.pend[i]);
                for (int i = 0; i < H.nonline; i++) H.online_fn[i](H.online_ctx[i]); /* e.g. sessions announce themselves again */
            } else {
                enter_backoff();
            }
            return;
        }
        for (int i = 0; i < MAX_PENDING; i++)
            if (H.pend[i].used && H.pend[i].id == (uint32_t)id) {
                finish(&H.pend[i], res_t >= 0 ? INK_HUB_OK : INK_HUB_ERR_REMOTE, j, res_t >= 0 ? res_t : err_t);
                return;
            }
        return;
    }
    int m_t = ink_json_find(j, 0, "method");
    if (m_t >= 0) {
        char method[40];
        if (ink_json_str(j, m_t, method, sizeof method) < 0) return;
        int params = ink_json_find(j, 0, "params");
        if (id_t >= 0) { /* a request from the hub (e.g. tools/call): exactly one handler answers */
            int64_t rid;
            if (!ink_json_int(j, id_t, &rid)) return;
            for (int i = 0; i < H.nreqs; i++)
                if (!strncmp(method, H.reqs[i].prefix, strlen(H.reqs[i].prefix))) {
                    H.reqs[i].fn(H.reqs[i].ctx, rid, method, j, params);
                    return;
                }
            ink_hub_respond_error(rid, -32601, "method not found on the device");
            return;
        }
        for (int i = 0; i < H.nsubs; i++)
            if (!strncmp(method, H.subs[i].prefix, strlen(H.subs[i].prefix))) H.subs[i].fn(H.subs[i].ctx, method, j, params);
    }
}

static void send_hello(void) {
    hal_device_info_t di;
    hal_device_get_info(&di);
    pending_t hello = {.id = new_id(), .used = false};
    H.hello_id = hello.id;
    char params[160];
    snprintf(params, sizeof params, "{\"proto\":1,\"device\":\"%s\",\"member\":\"default\"}", di.profile_id ? di.profile_id : "?");
    snprintf(hello.method, sizeof hello.method, "hello");
    hello.params = params;
    H.hello_sent_us = osal_now_us();
    if (send_request(&hello) != 0) enter_backoff();
    else set_state(HS_HELLO);
    hello.params = NULL;
}

static void release_tx(tx_item_t *it) {
    if (!it->pooled) {
        osal_free(it);
        return;
    }
    osal_mutex_lock(H.tx_lock, OSAL_WAIT_FOREVER);
    it->next = H.tx_free;
    H.tx_free = it;
    osal_mutex_unlock(H.tx_lock);
}

static void drain_tx(bool online) {
    for (;;) {
        osal_mutex_lock(H.tx_lock, OSAL_WAIT_FOREVER);
        tx_item_t *it = NULL;
        bool session = online && __atomic_load_n(&H.keep, __ATOMIC_ACQUIRE) > 0;
        bool paced = session && osal_now_us() < H.bg_next;
        int cls = 0;
        for (; cls < TX_CLASSES && !it; cls++)
            if (!(cls == INK_HUB_TX_BACKGROUND && paced) && (it = H.tx_head[cls])) {
                H.tx_head[cls] = it->next;
                if (!H.tx_head[cls]) H.tx_tail[cls] = NULL;
                H.tx_bytes -= it->len;
                break;
            }
        osal_mutex_unlock(H.tx_lock);
        if (!it) return; /* empty, or only background left while it is paced (next_wakeup) */
        if (!online || !H.ws || hal_ws_send(H.ws, it->binary, it->data, it->len, 2000000) != 0) {
            ink_probe_count("hub.tx_dropped");
        } else {
            H.last_activity = osal_now_us();
            if (session && cls == INK_HUB_TX_BACKGROUND) H.bg_next = H.last_activity + BG_GAP_US;
        }
        release_tx(it);
    }
}

/* ---------------------------------------------------------------- task */

static uint64_t next_wakeup(uint64_t now) {
    uint64_t t = UINT64_MAX;
    for (int i = 0; i < MAX_PENDING; i++)
        if (H.pend[i].used && H.pend[i].deadline < t) t = H.pend[i].deadline;
    if (H.state == HS_BACKOFF && H.backoff_until < t) t = H.backoff_until;
    if (H.state == HS_ONLINE) {
        uint64_t idle = H.last_activity + (low_power() ? 0 : IDLE_DISCONNECT_US), demand = demand_until();
        if (demand > idle) idle = demand;
        if (idle < t) t = idle;
    }
    if (H.state == HS_HELLO && H.connect_started + HELLO_TIMEOUT_US < t) t = H.connect_started + HELLO_TIMEOUT_US;
    if (H.state == HS_ONLINE && H.tx_head[INK_HUB_TX_BACKGROUND] && H.bg_next > now && H.bg_next < t) t = H.bg_next; /* paced */
    return t == UINT64_MAX ? 0 : (t > now ? t - now : 1);
}

/* A request enters the table of requests in flight (hub task). */
static void accept_call(call_req_t *r, uint64_t now) {
    pending_t *p = slot_new();
    if (!p) {
        if (r->cb) r->cb(r->ctx, INK_HUB_ERR_NOMEM, NULL, -1);
        osal_free(r->params);
    } else {
        memset(p, 0, sizeof *p);
        p->id = r->id;
        memcpy(p->method, r->method, sizeof p->method);
        p->params = r->params;
        p->deadline = now + (uint64_t)(r->timeout_us > 0 ? r->timeout_us : 20000000);
        p->cb = r->cb;
        p->ctx = r->ctx;
        p->used = true;
        H.last_activity = now;
        if (H.state == HS_ONLINE) send_request(p);
    }
    osal_free(r);
}

static void hub_task(void *arg) {
    (void)arg;
    for (;;) {
        uint64_t now = osal_now_us();
        uint64_t wait = next_wakeup(now);
        hub_msg_t m;
        osal_err_t e = osal_queue_recv(H.q, &m, wait ? (int64_t)wait : OSAL_WAIT_FOREVER);
        now = osal_now_us();
        if (e == OSAL_OK) {
            switch (m.type) {
            case HE_CALL: accept_call(m.req, now); break;
            case HE_DEMAND: break;
            case HE_LINK: {
                hal_link_status_t ls;
                hal_net_link_status(&ls);
                if (H.ws && ls.type != H.ws_link) { /* moved to another network: not waiting to notice that this one is dead */
                    fail_all(INK_HUB_ERR_LOST, true);
                    close_ws();
                    set_state(HS_DISCONNECTED);
                    ink_probe_count("hub.link_switched");
                }
                break;
            }
            case HE_TX: break; /* drained below */
            case HE_WS:
                if (!H.ws || m.ws_gen != H.ws_gen) break; /* from a connection already closed */
                if (m.ws_event == HAL_WS_CONNECTED && H.state == HS_CONNECTING) {
                    send_hello();
                } else if (m.ws_event == HAL_WS_TEXT && m.data) {
                    on_text(m.data, m.len);
                } else if (m.ws_event == HAL_WS_BINARY && m.data) {
                    H.last_activity = now;
                    if (H.bin_fn) H.bin_fn(H.bin_ctx, (const uint8_t *)m.data, m.len);
                } else if (m.ws_event == HAL_WS_CLOSED) {
                    fail_all(INK_HUB_ERR_LOST, true);
                    ink_probe_count("hub.disconnects");
                    if (has_demand(now)) enter_backoff();
                    else {
                        close_ws();
                        set_state(HS_DISCONNECTED);
                    }
                }
                break;
            }
            osal_free(m.data);
        }
        if (H.state == HS_ONLINE && H.tick) H.tick();
        /* queued notifications and binary frames: sent while online, dropped otherwise */
        drain_tx(H.state == HS_ONLINE);
        /* timeouts */
        for (int i = 0; i < MAX_PENDING; i++)
            if (H.pend[i].used && now >= H.pend[i].deadline)
                finish(&H.pend[i], H.pend[i].sent ? INK_HUB_ERR_TIMEOUT : INK_HUB_ERR_OFFLINE, NULL, -1);
        /* state machine */
        hal_link_status_t st;
        hal_net_link_status(&st);
        switch (H.state) {
        case HS_DISCONNECTED:
            if (has_demand(now) && st.up) start_connect();
            break;
        case HS_BACKOFF:
            if (!has_demand(now)) set_state(HS_DISCONNECTED);
            else if (now >= H.backoff_until && st.up) start_connect();
            break;
        case HS_HELLO:
            if (now >= H.connect_started + HELLO_TIMEOUT_US) enter_backoff();
            break;
        case HS_ONLINE:
            if (!has_demand(now) && now >= H.last_activity + (low_power() ? 0 : IDLE_DISCONNECT_US)) { /* D-24 */
                close_ws();
                set_state(HS_DISCONNECTED);
                ink_probe_count("hub.idle_disconnects");
            }
            break;
        default: break;
        }
    }
}

/* ----------------------------------------------------------------- API */

static void on_power(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us) { /* power task */
    (void)ctx;
    (void)from;
    (void)cause_us;
    __atomic_store_n(&H.low_power, to == INK_PWR_LOCKED, __ATOMIC_RELEASE); /* D-24: locked -> disconnect once quiet */
    hub_msg_t m = {.type = HE_DEMAND};   /* wake the hub task to re-evaluate (sets no demand) */
    osal_queue_send(H.q, &m, 0);
}

int ink_hub_init(const char *url) {
    snprintf(H.url, sizeof H.url, "%s", url);
    const char *prev = osal_mem_tag("hub");
    H.q = osal_queue_create(sizeof(hub_msg_t), HUB_QUEUE);
    H.id_lock = osal_mutex_create();
    H.json = osal_malloc(sizeof(ink_json_t), OSAL_MEM_EXTERNAL);
    H.tx_lock = osal_mutex_create();
    uint8_t *pool = osal_calloc(TXP_N, sizeof(tx_item_t) + TXP_PAYLOAD, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!H.q || !H.id_lock || !H.json || !H.tx_lock || !pool) return -1;
    for (int i = 0; i < TXP_N; i++) {
        tx_item_t *it = (tx_item_t *)(pool + (size_t)i * (sizeof(tx_item_t) + TXP_PAYLOAD));
        it->pooled = true;
        it->next = H.tx_free;
        H.tx_free = it;
    }
    ink_power_subscribe(on_power, NULL);
    hal_net_set_link_callback(link_cb, NULL);
    osal_task_cfg_t cfg = {.name = "hub", .fn = hub_task, .stack_bytes = HUB_STACK, .priority = HUB_PRIO, .core = -1,
                           .flags = OSAL_TASK_STACK_EXTERNAL};
    return osal_task_create(&cfg, &H.task) == OSAL_OK ? 0 : -1;
}

int ink_hub_call(const char *method, const char *params_json, int64_t timeout_us, ink_hub_reply_fn cb, void *ctx) {
    const char *prev = osal_mem_tag("hub");
    call_req_t *r = osal_calloc(1, sizeof *r, OSAL_MEM_EXTERNAL);
    const char *pj = params_json ? params_json : "{}";
    if (r) r->params = dup_mem(pj, strlen(pj));
    osal_mem_tag(prev);
    if (!r || !r->params) {
        if (r) osal_free(r);
        return INK_HUB_ERR_NOMEM;
    }
    r->id = new_id();
    snprintf(r->method, sizeof r->method, "%s", method);
    r->timeout_us = timeout_us;
    r->cb = cb;
    r->ctx = ctx;
    uint32_t id = r->id;
    if (osal_task_self() == H.task) { /* from a callback of this task: waiting for room in its own queue would never end */
        accept_call(r, osal_now_us());
        return (int)id;
    }
    hub_msg_t m = {.type = HE_CALL, .req = r};
    osal_queue_send(H.q, &m, OSAL_WAIT_FOREVER);
    return (int)id;
}

int ink_hub_subscribe(const char *prefix, ink_hub_notify_fn fn, void *ctx) {
    if (H.nsubs >= MAX_SUBS) return -1;
    snprintf(H.subs[H.nsubs].prefix, sizeof H.subs[0].prefix, "%s", prefix);
    H.subs[H.nsubs].fn = fn;
    H.subs[H.nsubs].ctx = ctx;
    H.nsubs++;
    return 0;
}

void ink_hub_demand(void) {
    if (!H.tx_lock) return; /* before ink_hub_init: nothing to connect */
    uint64_t until = osal_now_us() + DEMAND_HOLD_US;
    osal_mutex_lock(H.tx_lock, OSAL_WAIT_FOREVER);
    if (until > H.demand_until) H.demand_until = until;
    osal_mutex_unlock(H.tx_lock);
    hub_msg_t m = {.type = HE_DEMAND};
    osal_queue_send(H.q, &m, 0);
}

bool ink_hub_online(void) { return get_state() == HS_ONLINE; }

bool ink_hub_server_time(int64_t *unix_ms, uint32_t *round_trip_ms) {
    if (!H.server_ms) return false;
    *unix_ms = H.server_ms;
    *round_trip_ms = H.hello_rtt_ms;
    return true;
}

int ink_hub_on_online(void (*fn)(void *ctx), void *ctx) {
    if (H.nonline >= 4) return -1;
    H.online_ctx[H.nonline] = ctx;
    H.online_fn[H.nonline++] = fn;
    return 0;
}

void ink_hub_keep(bool on) {
    __atomic_add_fetch(&H.keep, on ? 1 : -1, __ATOMIC_ACQ_REL);
    ink_hub_demand(); /* connect now if needed; when the last one goes, the quiet period starts */
}

static int enqueue_tx(bool binary, int cls, const void *data, size_t len) {
    if (get_state() != HS_ONLINE || cls < 0 || cls >= TX_CLASSES) return -1;
    tx_item_t *it = NULL;
    osal_mutex_lock(H.tx_lock, OSAL_WAIT_FOREVER); /* reserve the budget first: producers race */
    bool full = H.tx_bytes + len > TX_MAX_BYTES;
    if (!full) {
        H.tx_bytes += len;
        if (binary && len <= TXP_PAYLOAD && H.tx_free) { /* a pooled item: no allocation per frame */
            it = H.tx_free;
            H.tx_free = it->next;
        }
    }
    osal_mutex_unlock(H.tx_lock);
    if (full) {
        ink_probe_count("hub.tx_overflow");
        return -1;
    }
    if (!it) {
        const char *prev = osal_mem_tag("hub");
        it = osal_malloc(sizeof *it + len, OSAL_MEM_EXTERNAL);
        osal_mem_tag(prev);
        if (it) it->pooled = false;
        else {
            osal_mutex_lock(H.tx_lock, OSAL_WAIT_FOREVER);
            H.tx_bytes -= len; /* give the reservation back */
            osal_mutex_unlock(H.tx_lock);
            return -1;
        }
    }
    it->next = NULL;
    it->binary = binary;
    it->len = len;
    memcpy(it->data, data, len);
    osal_mutex_lock(H.tx_lock, OSAL_WAIT_FOREVER);
    if (H.tx_tail[cls]) H.tx_tail[cls]->next = it;
    else H.tx_head[cls] = it;
    H.tx_tail[cls] = it;
    osal_mutex_unlock(H.tx_lock);
    hub_msg_t m = {.type = HE_TX};
    osal_queue_send(H.q, &m, 0); /* a full queue still wakes the task soon: it drains on every wake */
    return 0;
}

void ink_hub_set_tick(void (*fn)(void)) { H.tick = fn; }

void ink_hub_wake(void) {
    hub_msg_t m = {.type = HE_TX};
    if (H.q) osal_queue_send(H.q, &m, 0); /* a full queue wakes the task soon anyway */
}

int ink_hub_notify(const char *method, const char *params_json) {
    char head[80];
    const char *pj = params_json ? params_json : "{}";
    snprintf(head, sizeof head, "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":", method);
    size_t hl = strlen(head), pl = strlen(pj);
    const char *prev = osal_mem_tag("hub");
    char *buf = osal_malloc(hl + pl + 2, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!buf) return INK_HUB_ERR_NOMEM;
    memcpy(buf, head, hl);
    memcpy(buf + hl, pj, pl);
    buf[hl + pl] = '}';
    int r = enqueue_tx(false, INK_HUB_TX_CONTROL, buf, hl + pl + 1);
    osal_free(buf);
    return r == 0 ? 0 : INK_HUB_ERR_OFFLINE;
}

int ink_hub_send_binary(const void *data, size_t len) { return ink_hub_send_frame(data, len, INK_HUB_TX_VOICE); }

int ink_hub_send_frame(const void *data, size_t len, int cls) { return enqueue_tx(true, cls, data, len) == 0 ? 0 : INK_HUB_ERR_OFFLINE; }

void ink_hub_on_binary(ink_hub_binary_fn fn, void *ctx) {
    H.bin_ctx = ctx;
    H.bin_fn = fn;
}

int ink_hub_on_request(const char *prefix, ink_hub_request_fn fn, void *ctx) {
    if (H.nreqs >= 4) return -1;
    snprintf(H.reqs[H.nreqs].prefix, sizeof H.reqs[0].prefix, "%s", prefix);
    H.reqs[H.nreqs].fn = fn;
    H.reqs[H.nreqs].ctx = ctx;
    H.nreqs++;
    return 0;
}

int ink_hub_respond(int64_t id, const char *result_json) {
    const char *r = result_json ? result_json : "{}";
    size_t rl = strlen(r);
    const char *prev = osal_mem_tag("hub");
    char *buf = osal_malloc(rl + 64, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!buf) return INK_HUB_ERR_NOMEM;
    int n = snprintf(buf, 64, "{\"jsonrpc\":\"2.0\",\"id\":%lld,\"result\":", (long long)id);
    memcpy(buf + n, r, rl);
    buf[n + rl] = '}';
    int ok = enqueue_tx(false, INK_HUB_TX_CONTROL, buf, (size_t)n + rl + 1);
    osal_free(buf);
    return ok == 0 ? 0 : INK_HUB_ERR_OFFLINE;
}

int ink_hub_respond_error(int64_t id, int code, const char *message) {
    char buf[256];
    ink_jw_t w;
    ink_jw_init(&w, buf, sizeof buf);
    char head[80];
    snprintf(head, sizeof head, "{\"jsonrpc\":\"2.0\",\"id\":%lld,\"error\":{\"code\":%d,\"message\":", (long long)id, code);
    ink_jw_raw(&w, head);
    ink_jw_str(&w, message ? message : "error");
    ink_jw_raw(&w, "}}");
    if (w.overflow) return INK_HUB_ERR_NOMEM;
    return enqueue_tx(false, INK_HUB_TX_CONTROL, buf, w.len) == 0 ? 0 : INK_HUB_ERR_OFFLINE;
}
