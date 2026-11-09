/* Enrollment, independent of Wi-Fi. All flash work runs on exec.bg. */
#include <stdio.h>
#include <string.h>
#include "ink/hal_device.h"
#include "ink/hal_net.h"
#include "ink/hal_power.h"
#include "ink/hal_provision.h"
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/crc.h"
#include "ink/sys/data.h"
#include "ink/sys/file.h"
#include "ink/sys/hub.h"
#include "ink/sys/members.h"
#include "ink/sys/net.h"
#include "ink/sys/pairing.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/svc.h"

#define CONFIG_FILE "/user/sys/pair.config"
#define IDENTITY_FILE "/user/sys/pair.identity"
#define CARD_FILE "/content/ink-pair.json"
#define PAIR_VERSION 1u
#define MESSAGE_MAX 2048
#define COMMIT_RETRY_US 10000000ull

typedef struct {
    uint32_t version;
    char url[128], console[128], pin[65];
    uint32_t crc;
} pair_config_t;
typedef struct {
    uint32_t version, state;
    char nonce[33], txid[40], device[40], token[128], preauth[128];
    char family[64], by[64], name[64];
    uint32_t wipe;
    uint32_t crc;
} pair_identity_t;
typedef struct {
    ink_exec_job_t job;
    long long id;
    uint32_t web;
    int kind, status;
    uint32_t generation;
    bool trusted;
    char text[];
} pair_event_t;
enum { PE_BEGIN, PE_HELLO, PE_ACCEPT, PE_OFFER, PE_COMPLETE, PE_EXPIRED, PE_WEB };
static struct {
    bool ready;
    osal_sem_t *saved;
    pair_config_t config;
    pair_identity_t identity;
    bool enabled, blocked, halted, hotspot;
    char hotspot_owner[16];
    int save_result, queued, h_state, last_error;
    uint32_t generation;
    unsigned char peer[32];
    bool peer_valid;
    char code[9];
    uint64_t expires, retry_at;
    bool card_wifi;
    ink_exec_job_t card_job;
} P INK_PSRAM_BSS;

static const char *const STATES[] = {"unpaired", "pairing", "committing", "bound", "revoked"};
static void lock(void) {
    ink_svc_lock();
}
static void unlock(void) {
    ink_svc_unlock();
}
static void snapshot(pair_identity_t *s, pair_config_t *c) {
    lock();
    if (s) *s = P.identity;
    if (c) *c = P.config;
    unlock();
}
static void saved(void *ctx, int result) {
    (void) ctx;
    P.save_result = result;
    osal_sem_give(P.saved);
}
static bool durable(const char *path, void *data, size_t size) {
    if (!P.saved) P.saved = osal_sem_create(0, 1); /* background writer only */
    if (!P.saved) return false;
    uint32_t crc = ink_crc32(0, data, size - sizeof crc);
    memcpy((char *) data + size - sizeof crc, &crc, sizeof crc);
    bool ok = ink_storage_write_borrow(path, data, size, saved, NULL, NULL) == 0 &&
              osal_sem_take(P.saved, OSAL_WAIT_FOREVER) == OSAL_OK && P.save_result == 0;
    osal_sem_delete(P.saved);
    P.saved = NULL;
    return ok;
}
static void publish(void) {
    ink_state_set_str(P.h_state, STATES[ink_pairing_state()]);
    ink_args_t a;
    ink_args_init(&a);
    ink_args_str(&a, "state", STATES[ink_pairing_state()]);
    ink_cap_emit("pairing.changed", &a);
}
static bool store_identity(pair_identity_t *s) {
    s->version = PAIR_VERSION;
    if (!durable(IDENTITY_FILE, s, sizeof *s)) {
        ink_probe_count("pairing.save_failed");
        return false;
    }
    lock();
    P.identity = *s;
    P.retry_at = s->state == INK_PAIR_COMMITTING && !s->token[0] ? osal_now_us() + COMMIT_RETRY_US : 0;
    if (s->state == INK_PAIR_BOUND || s->state == INK_PAIR_REVOKED) P.code[0] = 0;
    unlock();
    publish();
    return true;
}
static void fresh(pair_identity_t *s) {
    unsigned char bytes[16];
    memset(s, 0, sizeof *s);
    s->version = PAIR_VERSION;
    hal_random_fill(bytes, sizeof bytes);
    for (unsigned i = 0; i < sizeof bytes; i++) snprintf(s->nonce + i * 2, 3, "%02x", bytes[i]);
}
static bool hex_key(const char *s, unsigned char out[32]) {
    if (strlen(s) != 64) return false;
    for (int i = 0; i < 32; i++) {
        unsigned v = 0;
        for (int k = 0; k < 2; k++) {
            char c = s[i * 2 + k];
            unsigned n = c >= '0' && c <= '9'   ? (unsigned) (c - '0')
                         : c >= 'a' && c <= 'f' ? (unsigned) (c - 'a' + 10)
                                                : 16;
            if (n > 15) return false;
            v = v * 16 + n;
        }
        out[i] = (unsigned char) v;
    }
    return true;
}
static bool normalize_key(char pin[65]) {
    static const char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char bytes[32];
    if (hex_key(pin, bytes)) return true;
    if (strncmp(pin, "sha256/", 7)) return false;
    const char *s = pin + 7;
    size_t n = strlen(s);
    if (n != 43 && !(n == 44 && s[43] == '=')) return false;
    unsigned value = 0, bits = 0, count = 0;
    for (int i = 0; i < 43; i++) {
        const char *p = strchr(ALPHABET, s[i]);
        if (!p) return false;
        value = (value << 6) | (unsigned) (p - ALPHABET);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (count >= 32) return false;
            bytes[count++] = (unsigned char) (value >> bits);
        }
    }
    if (count != 32 || (value & 3)) return false;
    for (int i = 0; i < 32; i++) snprintf(pin + i * 2, 3, "%02x", bytes[i]);
    return true;
}
/* Endpoints cannot contain credentials, fragments, whitespace or a query. */
static bool endpoint_ok(const char *s, const char *scheme, bool device) {
    size_t prefix = strlen(scheme), n = strlen(s);
    if (n <= prefix || n >= 128 || strncmp(s, scheme, prefix)) return false;
    const char *host = s + prefix, *slash = strchr(host, '/');
    if (!slash) slash = s + n;
    if (slash == host) return false;
    for (const char *p = host; *p; p++)
        if ((unsigned char) *p <= 32 || *p == '@' || *p == '?' || *p == '#' || *p == '\\') return false;
    return !device || !strcmp(slash, "/v1/device");
}
static bool token_string(const ink_json_t *j, int t, char *out, size_t cap, bool required) {
    out[0] = 0;
    if (t < 0 || t >= j->ntok || j->tok[t].type != 4) return false;
    int raw = j->tok[t].end - j->tok[t].start;
    if (raw < 0 || raw > MESSAGE_MAX) return false;
    char *full = osal_malloc((size_t) raw + 1, OSAL_MEM_EXTERNAL);
    if (!full) return false;
    int n = ink_json_str(j, t, full, (size_t) raw + 1);
    bool ok = n >= (required ? 1 : 0) && (size_t) n < cap && !memchr(full, 0, (size_t) n);
    if (ok) memcpy(out, full, (size_t) n + 1);
    memset(full, 0, (size_t) raw + 1);
    osal_free(full);
    return ok;
}
static bool string(const ink_json_t *j, int obj, const char *key, char *out, size_t cap, bool required) {
    int t = ink_json_find(j, obj, key);
    if (t < 0) {
        out[0] = 0;
        return !required;
    }
    return token_string(j, t, out, cap, required);
}
static void fail(ink_call_t *c, const char *why) {
    c->rc.status = INK_RC_FAILED;
    snprintf(c->rc.say, sizeof c->rc.say, "%s", why);
}
static bool by_person(ink_call_t *c) {
    if (c->src == INK_SRC_TOUCH || c->src == INK_SRC_KEY) return true;
    c->rc.status = INK_RC_REJECTED;
    snprintf(c->rc.say, sizeof c->rc.say, "请在设备上操作");
    return false;
}

bool ink_pairing_enabled(void) {
    if (!P.ready) return false;
    lock();
    bool v = P.enabled;
    unlock();
    return v;
}
bool ink_pairing_blocked(void) {
    return __atomic_load_n(&P.blocked, __ATOMIC_ACQUIRE);
}
ink_pair_state_t ink_pairing_state(void) {
    if (!P.ready) return INK_PAIR_UNPAIRED;
    lock();
    unsigned s = P.identity.state;
    unlock();
    return s <= INK_PAIR_REVOKED ? (ink_pair_state_t) s : INK_PAIR_UNPAIRED;
}
bool ink_pairing_wants_link(void) {
    return ink_pairing_enabled() && !ink_pairing_blocked() && ink_pairing_state() != INK_PAIR_BOUND;
}
bool ink_pairing_endpoint(char url[128], unsigned char pin[32], bool *pinned) {
    if (!ink_pairing_enabled()) return false;
    lock();
    memcpy(url, P.config.url, 128);
    *pinned = P.config.pin[0] && hex_key(P.config.pin, pin);
    unlock();
    return true;
}
void ink_pairing_peer(const unsigned char hash[32]) {
    if (!P.ready) return;
    lock();
    P.peer_valid = hash != NULL;
    if (hash)
        memcpy(P.peer, hash, 32);
    else
        memset(P.peer, 0, sizeof P.peer);
    unlock();
}
/* The caller holds the service lock; begin never grants removal authority. */
static bool trusted_hello(void) {
    unsigned char key[32];
    return P.peer_valid && (P.identity.state == INK_PAIR_BOUND || P.identity.state == INK_PAIR_COMMITTING) &&
           P.identity.token[0] && P.identity.device[0] && hex_key(P.config.pin, key) &&
           !memcmp(key, P.peer, sizeof key);
}
int ink_pairing_handshake(char method[32], char *params, size_t cap) {
    if (!ink_pairing_enabled() || ink_pairing_blocked()) return -1;
    ink_jw_t w;
    ink_jw_init(&w, params, cap);
    ink_member_identity_t identity;
    ink_member_identity(&identity);
    lock();
    pair_identity_t *s = &P.identity;
    if (s->token[0]) {
        snprintf(method, 32, "hello");
        ink_jw_raw(&w, "{\"proto\":2,\"hw\":\"metalio-e-ink4-plus\",\"fw\":\"dev\",\"device\":");
        ink_jw_str(&w, s->device);
        ink_jw_raw(&w, ",\"token\":");
        ink_jw_str(&w, s->token);
        ink_jw_raw(&w, ",\"member\":");
        ink_jw_str(&w, identity.member);
        ink_jw_raw(&w, identity.authed ? ",\"authed\":true" : ",\"authed\":false");
        ink_jw_raw(&w, ",\"epoch\":");
        ink_jw_int(&w, identity.epoch);
        ink_jw_raw(&w, "}");
    } else {
        snprintf(method, 32, "pair/begin");
        ink_jw_raw(&w, "{\"proto\":2,\"hw\":\"metalio-e-ink4-plus\",\"fw\":\"dev\",\"nonce\":");
        ink_jw_str(&w, s->nonce);
        if (s->state == INK_PAIR_COMMITTING && s->txid[0]) {
            ink_jw_raw(&w, ",\"txid\":");
            ink_jw_str(&w, s->txid);
        }
        if (s->preauth[0]) {
            ink_jw_raw(&w, ",\"preauth\":");
            ink_jw_str(&w, s->preauth);
        }
        ink_jw_raw(&w, "}");
    }
    unlock();
    return w.overflow ? -1 : 0;
}

static void tx_params(const char *txid, char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "{\"txid\":");
    ink_jw_str(&w, txid);
    ink_jw_raw(&w, "}");
}
static void event_job(void *arg);
static void free_event(pair_event_t *e) {
    memset(e->text, 0, strlen(e->text));
    osal_free(e);
    __atomic_sub_fetch(&P.queued, 1, __ATOMIC_ACQ_REL);
}
static bool queue_event(int kind, long long id, uint32_t web, int status, const char *text, size_t len,
                        uint32_t generation) {
    if (!P.ready || len > MESSAGE_MAX) return false;
    if (__atomic_add_fetch(&P.queued, 1, __ATOMIC_ACQ_REL) > 4) {
        __atomic_sub_fetch(&P.queued, 1, __ATOMIC_ACQ_REL);
        return false;
    }
    pair_event_t *e = osal_calloc(1, sizeof *e + len + 1, OSAL_MEM_EXTERNAL);
    if (!e) {
        __atomic_sub_fetch(&P.queued, 1, __ATOMIC_ACQ_REL);
        return false;
    }
    e->kind = kind;
    e->id = id;
    e->web = web;
    e->status = status;
    lock();
    e->generation = generation ? generation : P.generation;
    e->trusted = kind == PE_HELLO && trusted_hello();
    unlock();
    if (text) memcpy(e->text, text, len);
    if (ink_exec_post(INK_EXEC_BG, &e->job, event_job, e) == 0) return true;
    memset(e->text, 0, len);
    osal_free(e);
    __atomic_sub_fetch(&P.queued, 1, __ATOMIC_ACQ_REL);
    return false;
}
void ink_pairing_reply(bool hello, int status, const ink_json_t *j, int result) {
    lock();
    bool connected = P.peer_valid;
    unlock();
    if (!connected) return;
    int64_t code = 0;
    if (status < 0 && j) ink_json_int(j, ink_json_find(j, result, "code"), &code);
    lock();
    bool trusted = hello && trusted_hello();
    unlock();
    if ((code == -32011 && trusted) || code == -32012 ||
        (ink_pairing_state() == INK_PAIR_COMMITTING && (code == -32013 || code == -32010)))
        __atomic_store_n(&P.blocked, true, __ATOMIC_RELEASE);
    const char *txt = j ? j->text : NULL;
    size_t len = txt ? strlen(txt) : 0;
    if (!queue_event(hello ? PE_HELLO : PE_BEGIN, 0, 0, status, txt, len, 0) && ink_pairing_enabled())
        __atomic_store_n(&P.blocked, true, __ATOMIC_RELEASE);
}
bool ink_pairing_request(long long id, const char *method, const ink_json_t *j, int params) {
    (void) params;
    int kind = !strcmp(method, "pair/offer") ? PE_OFFER : !strcmp(method, "pair/complete") ? PE_COMPLETE : -1;
    if (kind < 0) return false;
    lock();
    bool connected = P.peer_valid;
    unlock();
    if (!connected || !ink_pairing_enabled() || ink_pairing_blocked() ||
        !queue_event(kind, id, 0, 0, j->text, strlen(j->text), 0))
        ink_hub_pair_respond_error(id, -32022, "pairing unavailable");
    return true;
}
void ink_pairing_expired(void) {
    if (queue_event(PE_EXPIRED, 0, 0, 0, NULL, 0, 0)) {
        lock();
        P.code[0] = 0;
        if (P.retry_at) P.retry_at = osal_now_us() + COMMIT_RETRY_US;
        unlock();
    }
}

static void accept_reply(void *ctx, int status, const ink_json_t *j, int result) {
    (void) result;
    if (status == INK_HUB_OK) return; /* Complete has its own progress deadline. */
    const char *text = j ? j->text : NULL;
    queue_event(PE_ACCEPT, 0, 0, status, text, text ? strlen(text) : 0, (uint32_t) (uintptr_t) ctx);
}
static void submit_accept(const pair_identity_t *s) {
    char params[256];
    tx_params(s->txid, params, sizeof params);
    lock();
    uint32_t generation = P.generation;
    unlock();
    void *ctx = (void *) (uintptr_t) generation;
    int rc = ink_hub_call("pair/accept", params, COMMIT_RETRY_US, accept_reply, ctx);
    if (rc < 0) accept_reply(ctx, rc, NULL, -1);
    ink_hub_demand();
}

/* Clear old authorization before changing its trust anchor on flash. */
static bool configure(const ink_json_t *j, int obj, const char *preauth) {
    pair_config_t *c = osal_calloc(1, sizeof *c, OSAL_MEM_EXTERNAL);
    pair_identity_t *s = osal_malloc(sizeof *s, OSAL_MEM_EXTERNAL);
    bool ok = c && s;
    if (ok) {
        snapshot(s, NULL);
        c->version = PAIR_VERSION;
        ok = string(j, obj, "url", c->url, sizeof c->url, true) && endpoint_ok(c->url, "wss://", true) &&
             string(j, obj, "console", c->console, sizeof c->console, false) &&
             string(j, obj, "pin", c->pin, sizeof c->pin, false) && (!c->pin[0] || normalize_key(c->pin));
        if (ok && !c->console[0]) {
            const char *host = c->url + 6, *end = strchr(host, '/');
            size_t n = end ? (size_t) (end - host) : strlen(host);
            /* The default console uses port 443, not the device channel's port. */
            const char *colon = memchr(host, ':', n);
            if (colon && *host != '[') n = (size_t) (colon - host);
            snprintf(c->console, sizeof c->console, "https://%.*s", (int) n, host);
        }
        ok = ok && endpoint_ok(c->console, "https://", false);
        lock();
        bool old = P.enabled, same = !strcmp(P.config.pin, c->pin) && c->pin[0];
        unlock();
        ok = ok && !s->wipe;
        if (s->state == INK_PAIR_BOUND || s->state == INK_PAIR_COMMITTING)
            ok = ok && old && same && (!preauth || !preauth[0]);
        if (ok) {
            __atomic_store_n(&P.blocked, true, __ATOMIC_RELEASE);
            lock();
            ++P.generation;
            P.peer_valid = false;
            P.code[0] = 0;
            P.expires = 0;
            unlock();
            if (!old || !same || s->state == INK_PAIR_REVOKED) {
                fresh(s);
                ok = store_identity(s);
            }
        }
        if (ok) ok = durable(CONFIG_FILE, c, sizeof *c);
        if (ok) {
            lock();
            P.config = *c;
            P.enabled = true;
            unlock();
            if (preauth && preauth[0]) snprintf(s->preauth, sizeof s->preauth, "%s", preauth);
            ok = store_identity(s);
            __atomic_store_n(&P.blocked, !ok || s->state == INK_PAIR_REVOKED, __ATOMIC_RELEASE);
            if (ok) ink_hub_reconnect();
        }
    }
    if (s) memset(s, 0, sizeof *s);
    osal_free(s);
    osal_free(c);
    return ok;
}

static void web_done(void *ctx, const ink_receipt_t *rc) {
    uint32_t request = (uint32_t) (uintptr_t) ctx;
    if (rc->status == INK_RC_DONE) {
        if (ink_wifi_wait_saved(rc->revision) != 0) {
            hal_provision_reply(request, 503);
            return;
        }
        if (!request && P.card_wifi) {
            if (ink_fs_card_hold() == INK_FS_OK) {
                hal_fs_remove(CARD_FILE);
                ink_fs_card_release();
            }
            P.card_wifi = false;
            return;
        }
        hal_provision_reply(request, 200);
        hal_provision_stop();
        lock();
        P.hotspot = false;
        unlock();
    } else {
        if (!request) P.card_wifi = false;
        hal_provision_reply(request, 400);
    }
}
static bool join_wifi(const ink_json_t *j, int obj, uint32_t request) {
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    char ssid[33], password[65];
    bool ok = a && string(j, obj, "ssid", ssid, sizeof ssid, true) &&
              string(j, obj, "password", password, sizeof password, false);
    if (ok) {
        ink_args_init(a);
        ok = ink_args_str(a, "ssid", ssid) == 0 && ink_args_str(a, "password", password) == 0;
        char owner[16];
        lock();
        memcpy(owner, P.hotspot_owner, sizeof owner);
        unlock();
        ink_call_opts_t opts = {.src = INK_SRC_TOUCH,
                                .reply_exec = INK_EXEC_BG,
                                .done = web_done,
                                .ctx = (void *) (uintptr_t) request,
                                .member = request ? owner : NULL};
        if (ok) ok = ink_cap_call("wifi.connect", a, &opts) == 0;
    }
    memset(password, 0, sizeof password);
    if (a) memset(a, 0, sizeof *a);
    osal_free(a);
    return ok;
}
static void finish_web(pair_event_t *e, const ink_json_t *j) {
    if (ink_json_find(j, 0, "ssid") >= 0) {
        if (!join_wifi(j, 0, e->web)) hal_provision_reply(e->web, 400);
    } else {
        hal_provision_reply(e->web, 200);
        hal_provision_stop();
        lock();
        P.hotspot = false;
        unlock();
    }
}
static void web_configured(void *ctx, const ink_receipt_t *rc) {
    pair_event_t *e = ctx;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (rc->status == INK_RC_DONE && j && ink_json_parse(j, e->text, strlen(e->text)) == 0)
        finish_web(e, j);
    else
        hal_provision_reply(e->web, 400);
    osal_free(j);
    free_event(e);
}
static void provision(void *ctx, uint32_t request, const char *json, size_t len) {
    (void) ctx;
    if (!queue_event(PE_WEB, 0, request, 0, json, len, 0)) hal_provision_reply(request, 503);
}

static bool complete_config(const ink_json_t *j, int obj) {
    pair_config_t *c = osal_malloc(sizeof *c, OSAL_MEM_EXTERNAL);
    if (!c) return false;
    snapshot(NULL, c);
    char observed[65];
    lock();
    for (int i = 0; i < 32; i++) snprintf(observed + i * 2, 3, "%02x", P.peer[i]);
    unlock();
    lock();
    bool connected = P.peer_valid;
    unlock();
    bool ok = connected && (!c->pin[0] || !strcmp(c->pin, observed));
    int urls = ink_json_find(j, obj, "urls"), pins = ink_json_find(j, obj, "pins");
    if (urls >= 0)
        ok = ok && j->tok[urls].type == 2 && j->tok[urls].size >= 1 &&
             token_string(j, urls + 1, c->url, sizeof c->url, true) && endpoint_ok(c->url, "wss://", true);
    if (pins >= 0) {
        bool matched = false;
        if (j->tok[pins].type == 2 && j->tok[pins].size > 0 && j->tok[pins].size <= 2) {
            for (int i = 0; i < j->tok[pins].size; i++) {
                char key[65];
                if (token_string(j, pins + 1 + i, key, sizeof key, true) && normalize_key(key) &&
                    !strcmp(key, observed))
                    matched = true;
            }
        }
        ok = ok && matched;
    }
    if (ok) {
        snprintf(c->pin, sizeof c->pin, "%s", observed);
        ok = durable(CONFIG_FILE, c, sizeof *c);
        if (ok) {
            lock();
            P.config = *c;
            unlock();
        }
    }
    osal_free(c);
    return ok;
}

int ink_factory_reset_from_hub(void);
static void event_job(void *arg) {
    pair_event_t *e = arg;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    pair_identity_t *s = osal_malloc(sizeof *s, OSAL_MEM_EXTERNAL);
    lock();
    bool current = e->generation == P.generation;
    unlock();
    if (!j || !s || !current) goto done;
    snapshot(s, NULL);
    int obj = -1;
    if (e->text[0] && ink_json_parse(j, e->text, strlen(e->text)) == 0)
        obj = e->kind == PE_WEB ? 0
                                : ink_json_find(j, 0,
                                                e->status < 0          ? "error"
                                                : e->kind <= PE_ACCEPT ? "result"
                                                                       : "params");
    if (e->kind == PE_WEB) {
        if (obj < 0) {
            hal_provision_reply(e->web, 400);
            goto done;
        }
        char owner[16];
        lock();
        bool active = P.hotspot;
        memcpy(owner, P.hotspot_owner, sizeof owner);
        unlock();
        active =
            active && !strcmp(owner, ink_member_current()->id) && ink_member_exists(owner) && ink_member_space_open();
        if (!active) {
            hal_provision_reply(e->web, 403);
            goto done;
        }
        if (ink_json_find(j, 0, "url") >= 0) {
            ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
            bool ok = a != NULL;
            if (a) {
                ink_args_init(a);
                const char *keys[] = {"url", "console", "pin"};
                for (int k = 0; ok && k < 3; k++) {
                    if (ink_json_find(j, 0, keys[k]) < 0) continue;
                    char value[128];
                    ok = string(j, 0, keys[k], value, sizeof value, true) && ink_args_str(a, keys[k], value) == 0;
                }
                ink_call_opts_t opts = {
                    .src = INK_SRC_TOUCH, .member = owner, .reply_exec = INK_EXEC_BG, .done = web_configured, .ctx = e};
                if (ok) ok = ink_cap_call("pairing.configure", a, &opts) == 0;
            }
            osal_free(a);
            if (ok) {
                memset(s, 0, sizeof *s);
                osal_free(s);
                osal_free(j);
                return;
            }
            hal_provision_reply(e->web, 400);
        } else
            finish_web(e, j);
    } else if (e->kind == PE_EXPIRED) {
        if (s->state == INK_PAIR_COMMITTING && !s->token[0]) {
            ink_probe_count("pairing.commit_retry");
            ink_hub_reconnect();
        } else if (s->state != INK_PAIR_COMMITTING && s->state != INK_PAIR_BOUND && s->state != INK_PAIR_REVOKED) {
            fresh(s);
            if (store_identity(s)) ink_hub_reconnect();
        }
    } else if (e->kind == PE_HELLO || e->kind == PE_BEGIN || e->kind == PE_ACCEPT) {
        if (e->status < 0) {
            int64_t code = 0;
            if (obj >= 0) ink_json_int(j, ink_json_find(j, obj, "code"), &code);
            lock();
            P.last_error = (int) code;
            unlock();
            publish();
            if (e->kind == PE_ACCEPT && s->state == INK_PAIR_COMMITTING && !s->token[0]) {
                lock();
                P.retry_at = osal_now_us() + 1000000;
                unlock();
                ink_hub_wake();
            }
            if (code == -32011 && e->trusted) {
                bool wipe = false;
                int data = ink_json_find(j, obj, "data");
                int w = ink_json_find(j, data, "wipe");
                wipe = w >= 0 && j->tok[w].type == 8 && ink_json_eq(j, w, "true");
                fresh(s);
                s->state = INK_PAIR_REVOKED;
                s->wipe = wipe;
                if (store_identity(s)) {
                    ink_hub_reconnect();
                    if (wipe) ink_factory_reset_from_hub();
                }
            } else if (code == -32012) {
                lock();
                P.halted = true;
                unlock();
            } else if (s->state == INK_PAIR_COMMITTING && (code == -32010 || code == -32013)) {
                fresh(s);
                if (store_identity(s)) {
                    __atomic_store_n(&P.blocked, false, __ATOMIC_RELEASE);
                    ink_hub_reconnect();
                }
            } else if (code == -32030 || code == -32031) {
                fresh(s);
                if (store_identity(s)) ink_hub_reconnect();
            }
        } else if (e->kind == PE_HELLO) {
            lock();
            P.last_error = 0;
            unlock();
            if (s->state == INK_PAIR_COMMITTING && s->token[0]) {
                s->state = INK_PAIR_BOUND;
                s->preauth[0] = 0;
                store_identity(s);
            }
        } else if (obj >= 0) {
            lock();
            P.last_error = 0;
            unlock();
            char code[9];
            int64_t seconds = 0;
            bool valid = string(j, obj, "code", code, sizeof code, true) && strlen(code) == 8;
            for (int i = 0; valid && i < 8; i++)
                valid = (code[i] >= 'A' && code[i] <= 'Z') || (code[i] >= '2' && code[i] <= '9');
            if (valid && ink_json_int(j, ink_json_find(j, obj, "expires_s"), &seconds) && seconds > 0 &&
                seconds <= 600) {
                bool already_pairing = s->state == INK_PAIR_PAIRING;
                if (s->state == INK_PAIR_UNPAIRED) {
                    s->state = INK_PAIR_PAIRING;
                    if (!store_identity(s)) goto done;
                }
                lock();
                memcpy(P.code, code, sizeof code);
                P.expires = osal_now_us() + (uint64_t) seconds * 1000000;
                unlock();
                if (already_pairing) publish();
            }
        }
    } else if (e->kind == PE_OFFER && obj >= 0) {
        char txid[40];
        bool ok = string(j, obj, "txid", txid, sizeof txid, true) &&
                  (s->state == INK_PAIR_PAIRING || (s->state == INK_PAIR_COMMITTING && !strcmp(s->txid, txid)));
        if (ok && s->state == INK_PAIR_PAIRING) {
            snprintf(s->txid, sizeof s->txid, "%s", txid);
            ok = string(j, obj, "family", s->family, sizeof s->family, true) &&
                 string(j, obj, "by", s->by, sizeof s->by, true) &&
                 string(j, obj, "name", s->name, sizeof s->name, true) && store_identity(s);
        }
        if (ok) {
            ink_hub_pair_respond(e->id, "{\"shown\":true}");
            if (s->state == INK_PAIR_COMMITTING && !s->token[0]) submit_accept(s);
        } else
            ink_hub_pair_respond_error(e->id, -32602, "invalid pairing offer");
    } else if (e->kind == PE_COMPLETE && obj >= 0) {
        char txid[40];
        bool ok = string(j, obj, "txid", txid, sizeof txid, true) &&
                  ((s->state == INK_PAIR_COMMITTING && !strcmp(s->txid, txid)) ||
                   ((s->state == INK_PAIR_PAIRING || s->state == INK_PAIR_UNPAIRED) && s->preauth[0]));
        if (ok) {
            snprintf(s->txid, sizeof s->txid, "%s", txid);
            ok = string(j, obj, "device", s->device, sizeof s->device, true) &&
                 string(j, obj, "token", s->token, sizeof s->token, true);
        }
        if (ok) {
            s->state = INK_PAIR_COMMITTING;
            ok = complete_config(j, obj) && store_identity(s);
        }
        if (ok) {
            ink_probe_count("pairing.token_durable");
            ink_hub_pair_respond(e->id, "{\"ok\":true}");
            ink_hub_reconnect();
        } else
            ink_hub_pair_respond_error(e->id, -32602, "pairing not confirmed or not saved");
    }
done:
    if (s) memset(s, 0, sizeof *s);
    osal_free(s);
    osal_free(j);
    free_event(e);
}

static void cap_configure(ink_call_t *c) {
    char text[512];
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int n = ink_args_to_json(&c->args, text, sizeof text);
    bool ok = j && n > 0 && ink_json_parse(j, text, (size_t) n) == 0 && configure(j, 0, NULL);
    osal_free(j);
    if (!ok) fail(c, "家庭服务器设置没能保存，请检查地址、公钥和存储");
}
static void cap_accept(ink_call_t *c) {
    if (!by_person(c)) return;
    pair_identity_t *s = osal_malloc(sizeof *s, OSAL_MEM_EXTERNAL);
    if (!s) {
        fail(c, "内存不够，请重试");
        return;
    }
    snapshot(s, NULL);
    char txid[40];
    ink_args_get_str(&c->args, "txid", txid, sizeof txid);
    lock();
    bool valid = P.code[0] && osal_now_us() < P.expires;
    unlock();
    if (s->state != INK_PAIR_PAIRING || !s->txid[0] || strcmp(txid, s->txid) || !valid)
        fail(c, "加入请求已变化，请重新查看");
    else {
        s->state = INK_PAIR_COMMITTING;
        if (!store_identity(s))
            fail(c, "没能保存，请重试");
        else
            submit_accept(s);
    }
    memset(s, 0, sizeof *s);
    osal_free(s);
}
static void cap_reject(ink_call_t *c) {
    pair_identity_t *s = osal_malloc(sizeof *s, OSAL_MEM_EXTERNAL);
    if (!s) {
        fail(c, "内存不够，请重试");
        return;
    }
    snapshot(s, NULL);
    char txid[40];
    ink_args_get_str(&c->args, "txid", txid, sizeof txid);
    if (s->state != INK_PAIR_PAIRING || strcmp(txid, s->txid))
        fail(c, "加入请求已变化");
    else {
        char params[256];
        tx_params(s->txid, params, sizeof params);
        s->txid[0] = s->family[0] = s->by[0] = s->name[0] = 0;
        if (store_identity(s))
            ink_hub_pair_call("pair/reject", params);
        else
            fail(c, "没能保存，请重试");
    }
    memset(s, 0, sizeof *s);
    osal_free(s);
}
static void cap_restart(ink_call_t *c) {
    pair_identity_t *s = osal_malloc(sizeof *s, OSAL_MEM_EXTERNAL);
    if (!s) {
        fail(c, "内存不够，请重试");
        return;
    }
    snapshot(s, NULL);
    if (s->state == INK_PAIR_BOUND || s->state == INK_PAIR_COMMITTING || s->wipe)
        fail(c, "请先完成加入或清空这台设备");
    else {
        fresh(s);
        if (store_identity(s)) {
            lock();
            ++P.generation;
            P.peer_valid = false;
            P.code[0] = 0;
            P.halted = false;
            unlock();
            __atomic_store_n(&P.blocked, false, __ATOMIC_RELEASE);
            ink_hub_reconnect();
        } else
            fail(c, "没能保存，请重试");
    }
    memset(s, 0, sizeof *s);
    osal_free(s);
}
static void cap_hotspot(ink_call_t *c) {
    bool on = !strcmp(c->cap->id, "pairing.hotspot_start");
    if (on) {
        unsigned char bytes[8];
        char password[17];
        hal_random_fill(bytes, sizeof bytes);
        for (unsigned i = 0; i < sizeof bytes; i++) snprintf(password + i * 2, 3, "%02x", bytes[i]);
        if (hal_provision_start("ink-setup", password, provision, NULL) != 0) {
            fail(c, "没能开启设备热点");
            return;
        }
        /* The Wi-Fi QR payload is handed to the owner's UI only. */
        char result[128];
        snprintf(result, sizeof result, "{\"wifi_qr\":\"WIFI:T:WPA;S:ink-setup;P:%s;;\"}", password);
        if (c->src == INK_SRC_TOUCH || c->src == INK_SRC_KEY || c->src == INK_SRC_SYSTEM) ink_cap_result(c, result);
        memset(password, 0, sizeof password);
    } else
        hal_provision_stop();
    char owner[16];
    snprintf(owner, sizeof owner, "%s", ink_member_current()->id);
    lock();
    P.hotspot = on;
    memcpy(P.hotspot_owner, owner, sizeof owner);
    unlock();
}
static void cap_import(ink_call_t *c) {
    if (c->src != INK_SRC_SYSTEM && c->src != INK_SRC_CONFIRMED && !by_person(c)) return;
    if (ink_fs_card_hold() != INK_FS_OK) {
        fail(c, "请插入存储卡并退出 U 盘模式");
        return;
    }
    char *text = osal_calloc(1, MESSAGE_MAX + 1, OSAL_MEM_EXTERNAL);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    hal_file_t *f = text ? hal_fs_open(CARD_FILE, HAL_O_READ) : NULL;
    int64_t size = hal_fs_size(CARD_FILE);
    bool ok = f && j && size > 0 && size <= MESSAGE_MAX;
    if (ok) ok = hal_fs_read(f, text, (size_t) size) == size;
    if (f && hal_fs_close(f) != 0) ok = false;
    char preauth[128] = "";
    if (ok) ok = ink_json_parse(j, text, (size_t) size) == 0 && string(j, 0, "preauth", preauth, sizeof preauth, false);
    /* A card is a trusted source: it must carry the public key. */
    if (ok) {
        char key[65];
        /* A restart may have bound the device before optional Wi-Fi was saved. */
        bool bound = ink_pairing_state() == INK_PAIR_BOUND;
        ok =
            string(j, 0, "pin", key, sizeof key, true) && normalize_key(key) && configure(j, 0, bound ? NULL : preauth);
    }
    bool wifi = ok && ink_json_find(j, 0, "ssid") >= 0;
    if (wifi) {
        P.card_wifi = true;
        ok = join_wifi(j, 0, 0);
        if (!ok) P.card_wifi = false;
    }
    if (ok && !wifi) ok = hal_fs_remove(CARD_FILE) == 0;
    if (ok && wifi) {
        c->rc.status = INK_RC_QUEUED;
        snprintf(c->rc.say, sizeof c->rc.say, "配对信息已保存，正在连接 Wi-Fi");
    }
    if (!ok) fail(c, "没能导入配对文件，请检查文件和存储卡");
    memset(preauth, 0, sizeof preauth);
    if (text) memset(text, 0, MESSAGE_MAX + 1);
    osal_free(text);
    osal_free(j);
    ink_fs_card_release();
}
static void cap_status(ink_call_t *c) {
    char *out = osal_malloc(1024, OSAL_MEM_EXTERNAL);
    if (!out) {
        fail(c, "内存不够，请重试");
        return;
    }
    static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    static const char B32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    char fingerprint[23] = "", check[13] = "";
    ink_jw_t w;
    ink_jw_init(&w, out, 1024);
    lock();
    for (int i = 0; i < 22; i++) {
        int bit = i * 6, b = bit / 8, shift = bit % 8;
        unsigned v = (unsigned) P.peer[b] << 8 | (b + 1 < 16 ? P.peer[b + 1] : 0);
        fingerprint[i] = B64[(v >> (10 - shift)) & 63];
    }
    for (int i = 0; i < 12; i++) {
        int bit = i * 5, b = bit / 8, shift = bit % 8;
        unsigned v = (unsigned) P.peer[b] << 8 | P.peer[b + 1];
        check[i] = B32[(v >> (11 - shift)) & 31];
    }
    ink_jw_raw(&w, "{\"state\":");
    ink_jw_str(&w, STATES[P.identity.state]);
    ink_jw_raw(&w, P.hotspot ? ",\"hotspot\":true" : ",\"hotspot\":false");
    ink_jw_raw(&w, ink_pairing_blocked() ? ",\"blocked\":true" : ",\"blocked\":false");
    ink_jw_raw(&w, ",\"error\":");
    ink_jw_int(&w, P.last_error);
    ink_jw_raw(&w, ",\"family\":");
    ink_jw_str(&w, P.identity.family);
    ink_jw_raw(&w, ",\"by\":");
    ink_jw_str(&w, P.identity.by);
    ink_jw_raw(&w, ",\"txid\":");
    ink_jw_str(&w, P.identity.txid);
    ink_jw_raw(&w, ",\"code\":");
    ink_jw_str(&w, P.code);
    ink_jw_raw(&w, ",\"check\":");
    ink_jw_str(&w, P.code[0] ? check : "");
    char claim[256] = "";
    if (P.code[0]) snprintf(claim, sizeof claim, "%s/claim/%s#s=%s", P.config.console, P.code, fingerprint);
    ink_jw_raw(&w, ",\"claim\":");
    ink_jw_str(&w, claim);
    ink_jw_raw(&w, "}");
    unlock();
    if (!w.overflow)
        ink_cap_result(c, out);
    else
        fail(c, "状态太长");
    osal_free(out);
}
#define OWNER .expose = INK_EXPOSE_OWNER, .exec = INK_EXEC_BG, .effect = INK_FX_SYSTEM
#define CONTROL .exec = INK_EXEC_BG, .effect = INK_FX_SYSTEM
static const ink_cap_t CAPS[] = {
    {.id = "pairing.status",
     .title = "查看加入家庭的进度",
     .effect = INK_FX_READ,
     .child_ok = true,
     .exec = INK_EXEC_BG,
     .run = cap_status},
    {.id = "pairing.configure",
     .title = "设置家庭服务器地址和公钥",
     CONTROL,
     .input =
         "{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\",\"maxLength\":127},\"console\":{\"type\":"
         "\"string\",\"maxLength\":127},\"pin\":{\"type\":\"string\",\"maxLength\":64}},\"required\":[\"url\"]}",
     .run = cap_configure},
    {.id = "pairing.accept",
     .title = "确认加入屏幕上的家庭",
     OWNER,
     .input = "{\"type\":\"object\",\"properties\":{\"txid\":{\"type\":\"string\",\"maxLength\":39}},\"required\":["
              "\"txid\"]}",
     .run = cap_accept},
    {.id = "pairing.reject",
     .title = "取消加入这个家庭",
     CONTROL,
     .input = "{\"type\":\"object\",\"properties\":{\"txid\":{\"type\":\"string\",\"maxLength\":39}},\"required\":["
              "\"txid\"]}",
     .run = cap_reject},
    {.id = "pairing.restart", .title = "重新加入家庭", CONTROL, .run = cap_restart},
    {.id = "pairing.import",
     .title = "从存储卡导入配对文件",
     CONTROL,
     .ask_by_hand = true,
     .ask = "从存储卡加入文件中的家庭？",
     .run = cap_import},
    {.id = "pairing.hotspot_start", .title = "开启设备设置热点", CONTROL, .run = cap_hotspot},
    {.id = "pairing.hotspot_stop", .title = "关闭设备设置热点", CONTROL, .run = cap_hotspot},
    {.id = "pairing.changed",
     .title = "加入家庭的进度变了",
     .form = INK_FORM_EVENT,
     .effect = INK_FX_READ,
     .child_ok = true},
};
static const ink_state_def_t STATE = {.id = "pairing.state", .type = INK_ST_STR, .title = "加入家庭的进度"};
static void card_tick(void *arg);
static void card_job(void *arg) {
    (void) arg;
    lock();
    bool wipe = P.identity.wipe != 0;
    unlock();
    if (wipe) {
        ink_factory_reset_from_hub();
        return;
    }
    bool file = false;
    if (!P.card_wifi && ink_fs_card_hold() == INK_FS_OK) {
        file = hal_fs_size(CARD_FILE) > 0;
        ink_fs_card_release();
    }
    if (file) {
        ink_call_t *c = osal_calloc(1, sizeof *c, OSAL_MEM_EXTERNAL);
        if (c) {
            c->src = INK_SRC_SYSTEM;
            cap_import(c);
            osal_free(c);
        }
    }
}
static void card_tick(void *arg) {
    (void) arg;
    ink_exec_post(INK_EXEC_BG, &P.card_job, card_job, NULL);
}
void ink_pairing_start(void) {
    lock();
    bool wipe = P.identity.wipe != 0;
    unlock();
    bool file = false;
    if (ink_fs_card_hold() == INK_FS_OK) {
        file = hal_fs_size(CARD_FILE) > 0;
        ink_fs_card_release();
    }
    if (wipe || file) card_tick(NULL);
}
uint64_t ink_pairing_deadline(void) {
    if (!P.ready) return 0;
    lock();
    uint64_t t = P.blocked                                                         ? 0
                 : P.identity.state == INK_PAIR_COMMITTING && !P.identity.token[0] ? P.retry_at
                 : P.code[0] && P.identity.state == INK_PAIR_PAIRING               ? P.expires
                                                                                   : 0;
    unlock();
    return t;
}
int ink_pairing_init(void) {
    P.ready = true;
    P.generation = 1;
    P.h_state = ink_state_define(&STATE);
    P.identity.version = PAIR_VERSION;
    int n = ink_storage_read(CONFIG_FILE, &P.config, sizeof P.config);
    if (n >= 0 || ink_storage_size(CONFIG_FILE) >= 0 || ink_storage_size(IDENTITY_FILE) >= 0) {
        P.enabled = true;
        bool ok = n == sizeof P.config && P.config.version == PAIR_VERSION &&
                  P.config.crc == ink_crc32(0, &P.config, sizeof P.config - 4) &&
                  memchr(P.config.url, 0, sizeof P.config.url) &&
                  memchr(P.config.console, 0, sizeof P.config.console) && memchr(P.config.pin, 0, sizeof P.config.pin);
        unsigned char key[32];
        ok = ok && endpoint_ok(P.config.url, "wss://", true) && endpoint_ok(P.config.console, "https://", false) &&
             (!P.config.pin[0] || hex_key(P.config.pin, key));
        int m = ink_storage_read(IDENTITY_FILE, &P.identity, sizeof P.identity);
        ok = ok && m == sizeof P.identity && P.identity.version == PAIR_VERSION &&
             P.identity.crc == ink_crc32(0, &P.identity, sizeof P.identity - 4) && P.identity.state <= INK_PAIR_REVOKED;
        if (ok) {
            pair_identity_t *s = &P.identity;
            ok = memchr(s->nonce, 0, sizeof s->nonce) && memchr(s->txid, 0, sizeof s->txid) &&
                 memchr(s->device, 0, sizeof s->device) && memchr(s->token, 0, sizeof s->token) &&
                 memchr(s->preauth, 0, sizeof s->preauth) && memchr(s->family, 0, sizeof s->family) &&
                 memchr(s->by, 0, sizeof s->by) && memchr(s->name, 0, sizeof s->name);
            if (ok)
                ok = strlen(s->nonce) == 32 && (s->state != INK_PAIR_BOUND || (s->token[0] && s->device[0])) &&
                     (s->state != INK_PAIR_COMMITTING || s->txid[0]);
        }
        if (!ok) {
            memset(&P.config, 0, sizeof P.config);
            fresh(&P.identity);
            P.blocked = true;
            ink_probe_count("pairing.invalid_saved_state");
        } else
            P.blocked = P.identity.state == INK_PAIR_REVOKED;
    } else
        fresh(&P.identity);
    if (P.identity.state == INK_PAIR_COMMITTING && !P.identity.token[0]) P.retry_at = osal_now_us() + COMMIT_RETRY_US;
    ink_state_set_str(P.h_state, STATES[P.identity.state]);
    return ink_cap_register(CAPS, sizeof CAPS / sizeof CAPS[0]);
}
