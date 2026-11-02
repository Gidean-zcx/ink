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
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/hal_net.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/hub.h"

#define HUB_STACK 4096
#define HUB_PRIO 6
#define HUB_QUEUE 16
#define MAX_PENDING 16
#define MAX_SUBS 8
#define IDLE_DISCONNECT_US (60ull * 1000000ull)
#define DEMAND_HOLD_US (30ull * 1000000ull)
#define HELLO_TIMEOUT_US (10ull * 1000000ull)

typedef enum { HS_DISCONNECTED, HS_CONNECTING, HS_HELLO, HS_ONLINE, HS_BACKOFF } hub_state_t;
typedef enum { HE_CALL = 1, HE_WS, HE_LINK, HE_DEMAND, HE_TX } hub_ev_t;

/* Fire-and-forget messages (notifications, binary audio frames): a FIFO in PSRAM drained by the
 * hub task while online, so callers never touch the WebSocket handle owned by the hub task. */
typedef struct tx_item {
    struct tx_item *next;
    bool binary;
    size_t len;
    uint8_t data[];
} tx_item_t;
#define TX_MAX_BYTES (96 * 1024)

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
    osal_mutex_t *id_lock;
    hal_ws_t *ws;
    hub_state_t state;
    pending_t pend[MAX_PENDING];
    uint32_t next_id;
    uint32_t hello_id;
    uint64_t last_activity, demand_until, backoff_until, backoff_us, connect_started;
    struct {
        char prefix[16];
        ink_hub_notify_fn fn;
        void *ctx;
    } subs[MAX_SUBS];
    int nsubs;
    ink_json_t *json; /* parse buffer (PSRAM, ~2 KB): keeps the token array off the stack */
    osal_mutex_t *tx_lock;
    tx_item_t *tx_head, *tx_tail;
    size_t tx_bytes;
    ink_hub_binary_fn bin_fn;
    void *bin_ctx;
    struct {
        char prefix[16];
        ink_hub_request_fn fn;
        void *ctx;
    } reqs[4];
    int nreqs;
} H;

static char *dup_mem(const void *p, size_t n) {
    char *c = osal_malloc(n + 1, OSAL_MEM_EXTERNAL);
    if (!c) return NULL;
    memcpy(c, p, n);
    c[n] = 0;
    return c;
}

/* ------------------------------------------------------------ HAL glue */

static void ws_cb(void *ctx, hal_ws_event_t ev, const uint8_t *data, size_t len) { /* platform network task */
    (void)ctx;
    hub_msg_t m = {.type = HE_WS, .ws_event = (uint8_t)ev};
    if (data && len) {
        const char *prev = osal_mem_tag("hub");
        m.data = dup_mem(data, len);
        osal_mem_tag(prev);
        m.len = len;
    }
    osal_queue_send(H.q, &m, OSAL_WAIT_FOREVER);
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
    if (p->sent_at && status == INK_HUB_OK) ink_probe_value("hub.request_ms", (double)(osal_now_us() - p->sent_at) / 1000.0);
    if (status != INK_HUB_OK) {
        char key[48];
        snprintf(key, sizeof key, "hub.errors.%d", -status);
        ink_probe_count(key);
    }
    if (p->cb) p->cb(p->ctx, status, msg, tok);
    osal_free(p->params);
    p->params = NULL;
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
    H.state = HS_BACKOFF;
    ink_probe_count("hub.backoffs");
}

static bool has_demand(uint64_t now) {
    if (now < H.demand_until) return true;
    for (int i = 0; i < MAX_PENDING; i++)
        if (H.pend[i].used) return true;
    return false;
}

static void start_connect(void) {
    hal_link_status_t st;
    hal_net_link_status(&st);
    H.connect_started = osal_now_us();
    H.ws = hal_ws_open(H.url, ws_cb, NULL);
    H.state = H.ws ? HS_CONNECTING : HS_BACKOFF;
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
                H.state = HS_ONLINE;
                H.backoff_us = 0;
                ink_probe_value("hub.online_ms", (double)(osal_now_us() - H.connect_started) / 1000.0);
                for (int i = 0; i < MAX_PENDING; i++)
                    if (H.pend[i].used && !H.pend[i].sent) send_request(&H.pend[i]);
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
    if (send_request(&hello) != 0) enter_backoff();
    else H.state = HS_HELLO;
    hello.params = NULL;
}

static void drain_tx(bool online) {
    for (;;) {
        osal_mutex_lock(H.tx_lock, OSAL_WAIT_FOREVER);
        tx_item_t *it = H.tx_head;
        if (it) {
            H.tx_head = it->next;
            if (!H.tx_head) H.tx_tail = NULL;
            H.tx_bytes -= it->len;
        }
        osal_mutex_unlock(H.tx_lock);
        if (!it) return;
        if (!online || !H.ws || hal_ws_send(H.ws, it->binary, it->data, it->len, 2000000) != 0) {
            ink_probe_count("hub.tx_dropped");
        } else {
            H.last_activity = osal_now_us();
        }
        osal_free(it);
    }
}

/* ---------------------------------------------------------------- task */

static uint64_t next_wakeup(uint64_t now) {
    uint64_t t = UINT64_MAX;
    for (int i = 0; i < MAX_PENDING; i++)
        if (H.pend[i].used && H.pend[i].deadline < t) t = H.pend[i].deadline;
    if (H.state == HS_BACKOFF && H.backoff_until < t) t = H.backoff_until;
    if (H.state == HS_ONLINE) {
        uint64_t idle = H.last_activity + IDLE_DISCONNECT_US;
        if (H.demand_until > idle) idle = H.demand_until;
        if (idle < t) t = idle;
    }
    if (H.state == HS_HELLO && H.connect_started + HELLO_TIMEOUT_US < t) t = H.connect_started + HELLO_TIMEOUT_US;
    return t == UINT64_MAX ? 0 : (t > now ? t - now : 1);
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
            case HE_CALL: {
                call_req_t *r = m.req;
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
                break;
            }
            case HE_DEMAND: break;
            case HE_LINK: break;
            case HE_TX: break; /* drained below */
            case HE_WS:
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
                        H.state = HS_DISCONNECTED;
                    }
                }
                break;
            }
            osal_free(m.data);
        }
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
            if (!has_demand(now)) H.state = HS_DISCONNECTED;
            else if (now >= H.backoff_until && st.up) start_connect();
            break;
        case HS_HELLO:
            if (now >= H.connect_started + HELLO_TIMEOUT_US) enter_backoff();
            break;
        case HS_ONLINE:
            if (!has_demand(now) && now >= H.last_activity + IDLE_DISCONNECT_US) { /* D-24: quiet -> disconnect */
                close_ws();
                H.state = HS_DISCONNECTED;
                ink_probe_count("hub.idle_disconnects");
            }
            break;
        default: break;
        }
    }
}

/* ----------------------------------------------------------------- API */

int ink_hub_init(const char *url) {
    snprintf(H.url, sizeof H.url, "%s", url);
    const char *prev = osal_mem_tag("hub");
    H.q = osal_queue_create(sizeof(hub_msg_t), HUB_QUEUE);
    H.id_lock = osal_mutex_create();
    H.json = osal_malloc(sizeof(ink_json_t), OSAL_MEM_EXTERNAL);
    H.tx_lock = osal_mutex_create();
    osal_mem_tag(prev);
    if (!H.q || !H.id_lock || !H.json || !H.tx_lock) return -1;
    hal_net_set_link_callback(link_cb, NULL);
    osal_task_cfg_t cfg = {.name = "hub", .fn = hub_task, .stack_bytes = HUB_STACK, .priority = HUB_PRIO, .core = -1,
                           .flags = OSAL_TASK_STACK_EXTERNAL};
    return osal_task_create(&cfg, NULL) == OSAL_OK ? 0 : -1;
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
    H.demand_until = osal_now_us() + DEMAND_HOLD_US;
    hub_msg_t m = {.type = HE_DEMAND};
    osal_queue_send(H.q, &m, 0);
}

bool ink_hub_online(void) { return H.state == HS_ONLINE; }

static int enqueue_tx(bool binary, const void *data, size_t len) {
    if (H.state != HS_ONLINE) return -1;
    osal_mutex_lock(H.tx_lock, OSAL_WAIT_FOREVER);
    bool full = H.tx_bytes + len > TX_MAX_BYTES;
    osal_mutex_unlock(H.tx_lock);
    if (full) {
        ink_probe_count("hub.tx_overflow");
        return -1;
    }
    const char *prev = osal_mem_tag("hub");
    tx_item_t *it = osal_malloc(sizeof *it + len, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!it) return -1;
    it->next = NULL;
    it->binary = binary;
    it->len = len;
    memcpy(it->data, data, len);
    osal_mutex_lock(H.tx_lock, OSAL_WAIT_FOREVER);
    if (H.tx_tail) H.tx_tail->next = it;
    else H.tx_head = it;
    H.tx_tail = it;
    H.tx_bytes += len;
    osal_mutex_unlock(H.tx_lock);
    hub_msg_t m = {.type = HE_TX};
    osal_queue_send(H.q, &m, 0); /* a full queue still wakes the task soon: it drains on every wake */
    return 0;
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
    int r = enqueue_tx(false, buf, hl + pl + 1);
    osal_free(buf);
    return r == 0 ? 0 : INK_HUB_ERR_OFFLINE;
}

int ink_hub_send_binary(const void *data, size_t len) { return enqueue_tx(true, data, len) == 0 ? 0 : INK_HUB_ERR_OFFLINE; }

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
    int ok = enqueue_tx(false, buf, (size_t)n + rl + 1);
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
    return enqueue_tx(false, buf, w.len) == 0 ? 0 : INK_HUB_ERR_OFFLINE;
}
