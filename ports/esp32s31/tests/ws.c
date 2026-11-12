/* Real HAL and SHA256; fake IDF TLS/transport boundary, no network or device. */
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fake.h"
#include "ink/hal_net.h"
#include "ink/osal.h"
#include "ink/sys/sha256.h"

struct osal_mutex { pthread_mutex_t value; };
struct osal_sem { pthread_mutex_t lock; pthread_cond_t changed; unsigned count; };
struct transport { void *ctx; struct transport *parent; connect_func connect; io_read_func read;
    io_func write; trans_func destroy; poll_func poll; int op, payload; bool fin; };
typedef struct { const uint8_t *data; int len, pos, op; bool fin; } frame_t;
static frame_t frames[8];
static unsigned head, tail;
static pthread_mutex_t events_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t events = PTHREAD_COND_INITIALIZER;
static pthread_t worker;
static int connected, closed, received, handshakes, allocations, destroyed, writes, tls_calls;
static bool trust_ca = true, fail_tls, peer_closed, expect_binary;
static uint8_t spki[] = "synthetic DER SPKI for host fixture";
static uint8_t *payload, *transmitted;
static size_t payload_len, transmitted_len;
static const void *caller_data;
static uint8_t expected[32], observed[32];
static osal_task_cfg_t job;

static struct timespec deadline_ms(int ms) {
    struct timespec until;
    assert(clock_gettime(CLOCK_REALTIME, &until) == 0);
    until.tv_nsec += (long)ms * 1000000;
    until.tv_sec += until.tv_nsec / 1000000000;
    until.tv_nsec %= 1000000000;
    return until;
}
static void wait_count(int *value, int target) {
    struct timespec until = deadline_ms(3000);
    pthread_mutex_lock(&events_lock);
    while (*value < target) assert(pthread_cond_timedwait(&events, &events_lock, &until) == 0);
    pthread_mutex_unlock(&events_lock);
}
static void wait_outcome(int target) {
    struct timespec until = deadline_ms(3000);
    pthread_mutex_lock(&events_lock);
    while (connected + closed < target) assert(pthread_cond_timedwait(&events, &events_lock, &until) == 0);
    pthread_mutex_unlock(&events_lock);
}
void *osal_malloc(size_t n, uint32_t caps) { (void)caps; return malloc(n); }
void *osal_calloc(size_t n, size_t size, uint32_t caps) { (void)caps; return calloc(n, size); }
void *osal_realloc(void *p, size_t n, uint32_t caps) { assert(caps == OSAL_MEM_EXTERNAL); return realloc(p, n); }
void osal_free(void *p) { free(p); }
osal_mutex_t *osal_mutex_create(void) {
    osal_mutex_t *m = malloc(sizeof *m); assert(m); pthread_mutex_init(&m->value, NULL); return m;
}
void osal_mutex_delete(osal_mutex_t *m) { pthread_mutex_destroy(&m->value); free(m); }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t us) { (void)us; return pthread_mutex_lock(&m->value) == 0 ? OSAL_OK : OSAL_TIMEOUT; }
void osal_mutex_unlock(osal_mutex_t *m) { pthread_mutex_unlock(&m->value); }
osal_sem_t *osal_sem_create(uint32_t n, uint32_t max) {
    (void)max; osal_sem_t *s = calloc(1, sizeof *s); assert(s);
    pthread_mutex_init(&s->lock, NULL); pthread_cond_init(&s->changed, NULL); s->count = n; return s;
}
void osal_sem_delete(osal_sem_t *s) { pthread_mutex_destroy(&s->lock); pthread_cond_destroy(&s->changed); free(s); }
osal_err_t osal_sem_take(osal_sem_t *s, int64_t us) {
    (void)us; pthread_mutex_lock(&s->lock);
    while (!s->count) pthread_cond_wait(&s->changed, &s->lock);
    s->count--; pthread_mutex_unlock(&s->lock); return OSAL_OK;
}
void osal_sem_give(osal_sem_t *s) {
    pthread_mutex_lock(&s->lock); s->count++; pthread_cond_signal(&s->changed); pthread_mutex_unlock(&s->lock);
}
uint64_t osal_now_us(void) { struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now); return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_nsec / 1000; }
void osal_task_yield(void) { sched_yield(); }
void osal_cost_units(const char *name, uint64_t n) { (void)name; (void)n; }
void osal_preempt_point(void) { }
static void *run(void *unused) { (void)unused; job.fn(job.arg); return NULL; }
osal_err_t osal_task_create(const osal_task_cfg_t *cfg, osal_task_t **out) {
    (void)out; assert(cfg->stack_bytes == 6144 && cfg->flags == OSAL_TASK_STACK_EXTERNAL); job = *cfg;
    return pthread_create(&worker, NULL, run, NULL) == 0 ? OSAL_OK : OSAL_NO_MEM;
}
esp_tls_t *esp_tls_init(void) { allocations++; return calloc(1, sizeof(esp_tls_t)); }
int esp_tls_conn_new_sync(const char *host, int n, int port, const esp_tls_cfg_t *cfg, esp_tls_t *tls) {
    assert(n == (int)strlen(host) && !strcmp(host, "example.com") && port == 8443 && cfg->timeout_ms == 10000);
    tls_calls++; mbedtls_ssl_config config = {0}; cfg->crt_bundle_attach(&config);
    tls->cert.pk_raw = (mbedtls_x509_buf){spki, sizeof spki};
    return !fail_tls && (config.mode == MBEDTLS_SSL_VERIFY_OPTIONAL || trust_ca) ? 1 : -1;
}
int esp_tls_conn_destroy(esp_tls_t *t) { allocations--; destroyed++; free(t); return 0; }
void *esp_tls_get_ssl_context(esp_tls_t *t) { return t; }
const mbedtls_x509_crt *mbedtls_ssl_get_peer_cert(const void *t) { return &((const esp_tls_t *)t)->cert; }
void mbedtls_ssl_conf_authmode(void *config, int mode) { ((mbedtls_ssl_config *)config)->mode = mode; }
int psa_hash_compute(int alg, const uint8_t *p, size_t n, uint8_t *out, size_t cap, size_t *len) {
    assert(alg == PSA_ALG_SHA_256 && cap == 32); ink_sha256(p, n, out); *len = 32; return PSA_SUCCESS;
}
int esp_crt_bundle_attach(void *conf) { ((mbedtls_ssl_config *)conf)->mode = 2; return 0; }
ssize_t esp_tls_get_bytes_avail(esp_tls_t *t) { (void)t; return 0; }
esp_err_t esp_tls_get_conn_sockfd(esp_tls_t *t, int *fd) { (void)t; *fd = 1; return 0; }
ssize_t esp_tls_conn_read(esp_tls_t *t, void *buf, size_t n) { (void)t; (void)buf; (void)n; return -1; }
ssize_t esp_tls_conn_write(esp_tls_t *t, const void *buf, size_t n) {
    (void)t; size_t take = n < 257 ? n : 257;
    transmitted = realloc(transmitted, transmitted_len + take); assert(transmitted);
    memcpy(transmitted + transmitted_len, buf, take); transmitted_len += take; writes++; return (ssize_t)take;
}
int ink_test_select(int n, fd_set *rd, fd_set *wr, fd_set *err, struct timeval *wait) {
    (void)n; (void)rd; (void)wr; (void)err; (void)wait; return 1;
}
int ink_test_setsockopt(int fd, int level, int option, const void *p, socklen_t n) {
    (void)fd; (void)level; (void)option; (void)p; (void)n; return 0;
}
esp_transport_handle_t esp_transport_init(void) { return calloc(1, sizeof(struct transport)); }
esp_err_t esp_transport_destroy(esp_transport_handle_t t) {
    if (t->destroy) t->destroy(t);
    free(t);
    return 0;
}
esp_err_t esp_transport_set_context_data(esp_transport_handle_t t, void *p) { t->ctx = p; return 0; }
void *esp_transport_get_context_data(esp_transport_handle_t t) { return t->ctx; }
esp_err_t esp_transport_set_func(esp_transport_handle_t t, connect_func c, io_read_func r, io_func w,
                               trans_func cl, poll_func pr, poll_func pw, trans_func d) {
    (void)cl; (void)pw; t->connect = c; t->read = r; t->write = w; t->poll = pr; t->destroy = d; return 0;
}
esp_transport_handle_t esp_transport_ws_init(esp_transport_handle_t parent) {
    esp_transport_handle_t t = esp_transport_init(); t->parent = parent; return t;
}
int esp_transport_ws_set_config(esp_transport_handle_t t, const esp_transport_ws_config_t *c) {
    (void)t; assert(!strcmp(c->ws_path, "/v1/device?test=1") && c->propagate_control_frames); return 0;
}
int esp_transport_connect(esp_transport_handle_t t, const char *host, int port, int ms) {
    int r = t->parent->connect(t->parent, host, port, ms);
    if (!r) handshakes++;
    return r;
}
int esp_transport_poll_read(esp_transport_handle_t t, int ms) {
    (void)t; struct timespec until = deadline_ms(ms);
    pthread_mutex_lock(&events_lock);
    if (head == tail && !peer_closed) pthread_cond_timedwait(&events, &events_lock, &until);
    int r = peer_closed ? -1 : (head < tail ? 1 : 0);
    pthread_mutex_unlock(&events_lock); return r;
}
int esp_transport_read(esp_transport_handle_t t, char *buf, int len, int ms) {
    (void)ms; pthread_mutex_lock(&events_lock); assert(head < tail);
    frame_t *f = &frames[head]; int n = f->len - f->pos; if (n > len) n = len;
    t->payload = f->len; t->op = f->op; t->fin = f->fin;
    if (n) memcpy(buf, f->data + f->pos, (size_t)n);
    f->pos += n;
    if (f->pos == f->len) head++;
    pthread_mutex_unlock(&events_lock); return n;
}
int esp_transport_ws_send_raw(esp_transport_handle_t t, int op, const char *buf, int len, int ms) {
    if ((op & 15) == WS_TRANSPORT_OPCODES_PONG) return len;
    assert((op & 15) == (expect_binary ? WS_TRANSPORT_OPCODES_BINARY : WS_TRANSPORT_OPCODES_TEXT));
    assert(op & WS_TRANSPORT_OPCODES_FIN); assert(buf != caller_data);
    return t->parent->write(t->parent, buf, len, ms);
}
int esp_transport_ws_get_read_opcode(esp_transport_handle_t t) { return t->op; }
int esp_transport_ws_get_read_payload_len(esp_transport_handle_t t) { return t->payload; }
bool esp_transport_ws_get_fin_flag(esp_transport_handle_t t) { return t->fin; }
void http_parser_url_init(struct http_parser_url *u) { memset(u, 0, sizeof *u); }
int http_parser_parse_url(const char *s, size_t n, int connect, struct http_parser_url *u) {
    (void)n; (void)connect; const char *host = strstr(s, "://"); if (!host) return -1;
    host += 3; const char *port = strchr(host, ':'), *path = strchr(host, '/');
    if (!port || !path) return -1;
    u->field_set = (1 << UF_HOST) | (1 << UF_PORT) | (1 << UF_PATH);
    u->field_data[UF_HOST].off = (uint16_t)(host - s); u->field_data[UF_HOST].len = (uint16_t)(port - host);
    u->field_data[UF_PATH].off = (uint16_t)(path - s); u->port = (uint16_t)atoi(port + 1); return 0;
}
static void callback(void *ctx, hal_ws_event_t ev, const uint8_t *data, size_t len) {
    hal_ws_t **w = ctx; pthread_mutex_lock(&events_lock);
    if (ev == HAL_WS_CONNECTED) { assert(hal_ws_peer_key(*w, observed)); connected++; }
    else if (ev == HAL_WS_CLOSED) closed++;
    else {
        assert(ev == (expect_binary ? HAL_WS_BINARY : HAL_WS_TEXT));
        assert(len == payload_len && !memcmp(data, payload, len)); received++;
    }
    pthread_cond_broadcast(&events); pthread_mutex_unlock(&events_lock);
}
static void queue_frame(const uint8_t *data, int len, int op, bool fin) {
    pthread_mutex_lock(&events_lock); assert(tail < sizeof frames / sizeof frames[0]);
    frames[tail++] = (frame_t){data, len, 0, op, fin}; pthread_cond_broadcast(&events); pthread_mutex_unlock(&events_lock);
}
static void release(hal_ws_t *w) { hal_ws_free(w); pthread_join(worker, NULL); assert(allocations == 0); }
int main(void) {
    const char *url = "wss://example.com:8443/v1/device?test=1";
    ink_sha256(spki, sizeof spki, expected);
    hal_ws_t *w = NULL;
    /* The callback reads the handle only after open returns. */
    pthread_mutex_lock(&events_lock);
    w = hal_ws_open_pinned(url, expected, callback, &w);
    assert(w); pthread_mutex_unlock(&events_lock);
    wait_count(&connected, 1); assert(!memcmp(observed, expected, 32));
    payload_len = 70000; payload = malloc(payload_len); assert(payload);
    for (size_t i = 0; i < payload_len; i++) payload[i] = (uint8_t)i;
    expect_binary = true; caller_data = payload;
    assert(hal_ws_send(w, true, payload, payload_len, 2000000) == 0);
    assert(transmitted_len == payload_len && !memcmp(transmitted, payload, payload_len) && writes > 1);
    assert(hal_ws_flush(w, 2000000) == 0);
    queue_frame(payload, 35000, WS_TRANSPORT_OPCODES_BINARY, false);
    queue_frame(payload, 3, WS_TRANSPORT_OPCODES_PING, true);
    queue_frame(payload + 35000, 35000, WS_TRANSPORT_OPCODES_CONT, true);
    wait_count(&received, 1);
    pthread_mutex_lock(&events_lock); peer_closed = true; pthread_cond_broadcast(&events); pthread_mutex_unlock(&events_lock);
    wait_count(&closed, 1); assert(hal_ws_send(w, true, payload, 1, 0) == -1); release(w);
    /* Wrong SPKI must fail before the HTTP upgrade, including after a disconnect. */
    peer_closed = false; expected[0] ^= 1; int before = handshakes;
    pthread_mutex_lock(&events_lock); w = hal_ws_open_pinned(url, expected, callback, &w); assert(w); pthread_mutex_unlock(&events_lock);
    wait_outcome(3); assert(closed == 2 && connected == 1 && handshakes == before); release(w); expected[0] ^= 1;
    trust_ca = false;
    w = hal_ws_open(url, callback, &w); assert(w); wait_count(&closed, 3); assert(handshakes == before); release(w);
    /* A pinned self-signed server still works; owner reconnects with a fresh handle. */
    pthread_mutex_lock(&events_lock); w = hal_ws_open_pinned(url, expected, callback, &w); assert(w); pthread_mutex_unlock(&events_lock);
    wait_count(&connected, 2); expect_binary = false; transmitted_len = 0;
    for (size_t i = 0; i < payload_len; i++) payload[i] = (uint8_t)('a' + i % 26);
    assert(hal_ws_send(w, false, payload, payload_len, 2000000) == 0);
    queue_frame(payload, (int)payload_len, WS_TRANSPORT_OPCODES_TEXT, true); wait_count(&received, 2);
    release(w); assert(closed == 3); /* intentional free emits no CLOSED */
    fail_tls = true;
    w = hal_ws_open_pinned(url, expected, callback, &w); assert(w); wait_count(&closed, 4); release(w);
    assert(destroyed == 5 && tls_calls == 5);
    free(payload); free(transmitted);
    puts("S31 WS: SPKI before upgrade, CA default, disconnect/reopen, 70KB text/binary, fragments/ping, free: passed");
    return 0;
}
