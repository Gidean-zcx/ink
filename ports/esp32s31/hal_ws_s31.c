/* ESP-IDF owns TLS and RFC6455 framing; the hub service owns reconnects. */
#include <limits.h>
#include <string.h>
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_transport.h"
#include "esp_transport_ws.h"
#include "http_parser.h"
#include "lwip/sockets.h"
#include "mbedtls/ssl.h"
#include "psa/crypto.h"
#include "ink/hal_net.h"
#include "ink/osal.h"

struct hal_ws {
    esp_tls_t *tls;
    esp_transport_handle_t transport, parent;
    osal_mutex_t *lock;
    osal_sem_t *done;
    hal_ws_cb cb;
    void *ctx;
    bool pinned, connected, stop;
    uint64_t write_until;
    uint8_t expected[32], peer[32];
    char url[128];
};

static bool ink_stopped(hal_ws_t *w) { return __atomic_load_n(&w->stop, __ATOMIC_ACQUIRE); }
static bool ink_connected(hal_ws_t *w) { return __atomic_load_n(&w->connected, __ATOMIC_ACQUIRE); }
static void ink_connection(hal_ws_t *w, bool value) { __atomic_store_n(&w->connected, value, __ATOMIC_RELEASE); }

/* This connection authenticates by SPKI below, independently of clock/CA/name. */
static esp_err_t ink_pin_auth(void *config) {
    mbedtls_ssl_conf_authmode(config, MBEDTLS_SSL_VERIFY_OPTIONAL);
    return ESP_OK;
}

static int ink_tls_connect(esp_transport_handle_t t, const char *host, int port, int ms) {
    hal_ws_t *w = esp_transport_get_context_data(t);
    esp_tls_cfg_t cfg = {.timeout_ms = ms,
        .crt_bundle_attach = w->pinned ? ink_pin_auth : esp_crt_bundle_attach};
    if (esp_tls_conn_new_sync(host, (int)strlen(host), port, &cfg, w->tls) != 1) return -1;
    const mbedtls_x509_crt *cert = mbedtls_ssl_get_peer_cert(esp_tls_get_ssl_context(w->tls));
    size_t digest_len = 0;
    if (!cert || !cert->pk_raw.p || !cert->pk_raw.len ||
            psa_hash_compute(PSA_ALG_SHA_256, cert->pk_raw.p, cert->pk_raw.len,
                             w->peer, sizeof w->peer, &digest_len) != PSA_SUCCESS || digest_len != sizeof w->peer) return -1;
    if (w->pinned && memcmp(w->expected, w->peer, sizeof w->peer)) {
        ESP_LOGW("ink_ws", "peer key rejected");
        return -1;
    }
    return 0; /* No HTTP bytes leave before the pin has passed. */
}

static int ink_poll(esp_transport_handle_t t, int ms, bool writing) {
    hal_ws_t *w = esp_transport_get_context_data(t);
    if (writing && w->write_until && w->write_until != UINT64_MAX) {
        uint64_t now = osal_now_us();
        if (now >= w->write_until) return 0;
        uint64_t left = (w->write_until - now + 999) / 1000;
        if (left < (uint64_t)INT_MAX && (ms < 0 || (uint64_t)ms > left)) ms = (int)left;
    }
    if (!writing && esp_tls_get_bytes_avail(w->tls) > 0) return 1;
    int fd = -1;
    if (esp_tls_get_conn_sockfd(w->tls, &fd) != ESP_OK || fd < 0) return -1;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval wait = {.tv_sec = ms < 0 ? 0 : ms / 1000, .tv_usec = ms < 0 ? 0 : ms % 1000 * 1000};
    return select(fd + 1, writing ? NULL : &fds, writing ? &fds : NULL, NULL, ms < 0 ? NULL : &wait);
}
static int ink_poll_read(esp_transport_handle_t t, int ms) { return ink_poll(t, ms, false); }
static int ink_poll_write(esp_transport_handle_t t, int ms) { return ink_poll(t, ms, true); }

static int ink_tls_read(esp_transport_handle_t t, char *buf, int len, int ms) {
    hal_ws_t *w = esp_transport_get_context_data(t);
    int ready = ink_poll_read(t, ms);
    if (ready <= 0) return ready;
    int fd = -1;
    struct timeval wait = {.tv_sec = ms < 0 ? 0 : ms / 1000, .tv_usec = ms < 0 ? 0 : ms % 1000 * 1000};
    if (esp_tls_get_conn_sockfd(w->tls, &fd) != ESP_OK ||
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof wait) != 0) return -1;
    int n = (int)esp_tls_conn_read(w->tls, buf, (size_t)len);
    return n > 0 ? n : -1;
}

static int ink_tls_write(esp_transport_handle_t t, const char *buf, int len, int ms) {
    hal_ws_t *w = esp_transport_get_context_data(t);
    uint64_t end = ms < 0 ? UINT64_MAX : osal_now_us() + (uint64_t)ms * 1000;
    int sent = 0;
    while (sent < len) {
        uint64_t now = osal_now_us();
        int left = end == UINT64_MAX ? -1 : (now >= end ? 0 : (int)((end - now + 999) / 1000));
        if (ink_poll_write(t, left) <= 0) return -1;
        int fd = -1;
        struct timeval wait = {.tv_sec = left < 0 ? 0 : left / 1000, .tv_usec = left < 0 ? 0 : left % 1000 * 1000};
        if (esp_tls_get_conn_sockfd(w->tls, &fd) != ESP_OK ||
                setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &wait, sizeof wait) != 0) return -1;
        int n = (int)esp_tls_conn_write(w->tls, buf + sent, (size_t)(len - sent));
        if (n <= 0) return -1;
        sent += n;
    }
    return sent;
}
static int ink_noop(esp_transport_handle_t t) { (void)t; return 0; }

static bool ink_receive(hal_ws_t *w, uint8_t *scratch, uint8_t **message, size_t *used,
                        int *remaining, int *kind) {
    int n = esp_transport_read(w->transport, (char *)scratch, 1024, 1000);
    if (n < 0) return false;
    int op = esp_transport_ws_get_read_opcode(w->transport);
    if (!*remaining) {
        *remaining = esp_transport_ws_get_read_payload_len(w->transport);
        if (op == WS_TRANSPORT_OPCODES_CLOSE) return false;
        if (op == WS_TRANSPORT_OPCODES_PING || op == WS_TRANSPORT_OPCODES_PONG) {
            *remaining = 0;
            return op != WS_TRANSPORT_OPCODES_PING ||
                esp_transport_ws_send_raw(w->transport, WS_TRANSPORT_OPCODES_PONG | WS_TRANSPORT_OPCODES_FIN,
                                          (char *)scratch, n, 1000) == n;
        }
        if (op == WS_TRANSPORT_OPCODES_TEXT || op == WS_TRANSPORT_OPCODES_BINARY) {
            if (*kind) return false;
            *kind = op;
        } else if (op != WS_TRANSPORT_OPCODES_CONT || !*kind) return false;
        if ((size_t)*remaining > SIZE_MAX - *used - 1) return false;
        uint8_t *next = osal_realloc(*message, *used + (size_t)*remaining + 1, OSAL_MEM_EXTERNAL);
        if (!next) return false;
        *message = next;
    }
    if (n > *remaining) return false;
    memcpy(*message + *used, scratch, (size_t)n);
    *used += (size_t)n;
    *remaining -= n;
    return true;
}

static void ink_ws_task(void *arg) {
    hal_ws_t *w = arg;
    struct http_parser_url url;
    http_parser_url_init(&url);
    char host[128], path[128];
    uint8_t *scratch = osal_malloc(1024, OSAL_MEM_EXTERNAL), *message = NULL;
    size_t used = 0;
    int remaining = 0, kind = 0;
    bool valid = scratch && !strncmp(w->url, "wss://", 6) &&
        http_parser_parse_url(w->url, strlen(w->url), 0, &url) == 0 &&
        (url.field_set & (1 << UF_HOST)) && !(url.field_set & ((1 << UF_USERINFO) | (1 << UF_FRAGMENT)));
    if (valid) {
        size_t n = url.field_data[UF_HOST].len;
        memcpy(host, w->url + url.field_data[UF_HOST].off, n);
        host[n] = 0;
        const char *start = url.field_set & (1 << UF_PATH) ? w->url + url.field_data[UF_PATH].off : NULL;
        if (start) memcpy(path, start, strlen(start) + 1);
        else if (url.field_set & (1 << UF_QUERY)) {
            path[0] = '/';
            memcpy(path + 1, w->url + url.field_data[UF_QUERY].off - 1,
                   strlen(w->url + url.field_data[UF_QUERY].off - 1) + 1);
        } else memcpy(path, "/", 2);
        esp_transport_ws_config_t config = {.ws_path = path, .propagate_control_frames = true};
        valid = esp_transport_ws_set_config(w->transport, &config) == ESP_OK &&
            esp_transport_connect(w->transport, host, url.field_set & (1 << UF_PORT) ? url.port : 443, 10000) >= 0;
    }
    if (valid && !ink_stopped(w)) {
        ink_connection(w, true);
        ESP_LOGI("ink_ws", "TLS and WebSocket connected");
        if (w->cb) w->cb(w->ctx, HAL_WS_CONNECTED, NULL, 0);
    }
    while (ink_connected(w) && !ink_stopped(w)) {
        osal_mutex_lock(w->lock, OSAL_WAIT_FOREVER);
        int ready = esp_transport_poll_read(w->transport, 20);
        bool ok = ready >= 0 && (!ready || ink_receive(w, scratch, &message, &used, &remaining, &kind));
        bool deliver = ok && ready && kind && !remaining && esp_transport_ws_get_fin_flag(w->transport) &&
            esp_transport_ws_get_read_opcode(w->transport) < WS_TRANSPORT_OPCODES_CLOSE;
        osal_mutex_unlock(w->lock);
        if (!ok) break;
        if (deliver) {
            if (w->cb && !ink_stopped(w)) w->cb(w->ctx, kind == WS_TRANSPORT_OPCODES_TEXT ? HAL_WS_TEXT : HAL_WS_BINARY, message, used);
            osal_free(message);
            message = NULL;
            used = 0;
            kind = 0;
        }
        osal_task_yield();
    }
    osal_mutex_lock(w->lock, OSAL_WAIT_FOREVER);
    ink_connection(w, false);
    esp_tls_conn_destroy(w->tls);
    w->tls = NULL;
    osal_mutex_unlock(w->lock);
    osal_free(message);
    osal_free(scratch);
    if (!ink_stopped(w)) {
        ESP_LOGI("ink_ws", "connection closed");
        if (w->cb) w->cb(w->ctx, HAL_WS_CLOSED, NULL, 0);
    }
    osal_sem_give(w->done);
}

hal_ws_t *hal_ws_open_pinned(const char *url, const uint8_t *sha256, hal_ws_cb cb, void *ctx) {
    hal_ws_t *w = osal_calloc(1, sizeof *w, OSAL_MEM_EXTERNAL);
    if (!w) return NULL;
    w->cb = cb;
    w->ctx = ctx;
    w->pinned = sha256 != NULL;
    if (sha256) memcpy(w->expected, sha256, 32);
    if (url && strlen(url) < sizeof w->url) memcpy(w->url, url, strlen(url) + 1);
    w->lock = osal_mutex_create();
    w->done = osal_sem_create(0, 1);
    w->tls = esp_tls_init();
    w->parent = esp_transport_init();
    w->transport = w->parent ? esp_transport_ws_init(w->parent) : NULL;
    if (w->lock && w->done && w->tls && w->transport) {
        esp_transport_set_context_data(w->parent, w);
        esp_transport_set_func(w->parent, ink_tls_connect, ink_tls_read, ink_tls_write, ink_noop,
                               ink_poll_read, ink_poll_write, ink_noop);
        osal_task_cfg_t cfg = {.name = "ws", .fn = ink_ws_task, .arg = w, .stack_bytes = 6144,
            .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
        if (osal_task_create(&cfg, NULL) == OSAL_OK) return w;
    }
    if (w->transport) esp_transport_destroy(w->transport);
    if (w->parent) esp_transport_destroy(w->parent);
    if (w->tls) esp_tls_conn_destroy(w->tls);
    if (w->done) osal_sem_delete(w->done);
    if (w->lock) osal_mutex_delete(w->lock);
    osal_free(w);
    return NULL;
}

hal_ws_t *hal_ws_open(const char *url, hal_ws_cb cb, void *ctx) { return hal_ws_open_pinned(url, NULL, cb, ctx); }
bool hal_ws_peer_key(hal_ws_t *w, uint8_t out[32]) {
    if (!w || !out || !ink_connected(w)) return false;
    memcpy(out, w->peer, 32);
    return true;
}
int hal_ws_send(hal_ws_t *w, bool binary, const void *data, size_t len, int64_t timeout_us) {
    if (!w || len > INT_MAX || (len && !data) || !ink_connected(w) || ink_stopped(w)) return -1;
    uint64_t start = osal_now_us();
    uint8_t *copy = osal_malloc(len ? len : 1, OSAL_MEM_EXTERNAL);
    if (!copy) return -1;
    if (len) memcpy(copy, data, len);
    int result = -1;
    if (osal_mutex_lock(w->lock, timeout_us) == OSAL_OK) {
        uint64_t elapsed = osal_now_us() - start;
        int64_t left = timeout_us < 0 ? -1 : (elapsed >= (uint64_t)timeout_us ? 0 : timeout_us - (int64_t)elapsed);
        int ms = left < 0 ? -1 : (int)((left / 1000) > INT_MAX - 1 ? INT_MAX : (left + 999) / 1000);
        if (ink_connected(w) && !ink_stopped(w)) {
            int op = (binary ? WS_TRANSPORT_OPCODES_BINARY : WS_TRANSPORT_OPCODES_TEXT) | WS_TRANSPORT_OPCODES_FIN;
            w->write_until = timeout_us < 0 ? UINT64_MAX : start + (uint64_t)timeout_us;
            result = esp_transport_ws_send_raw(w->transport, op, (char *)copy, (int)len, ms) == (int)len ? 0 : -1;
            w->write_until = 0;
            if (result) ink_connection(w, false); /* A partial write cannot be reused. */
        }
        osal_mutex_unlock(w->lock);
    }
    osal_free(copy);
    return result;
}
int hal_ws_flush(hal_ws_t *w, int64_t timeout_us) {
    if (!w || timeout_us < 0 || osal_mutex_lock(w->lock, timeout_us) != OSAL_OK) return -1;
    int result = ink_connected(w) && !ink_stopped(w) ? 0 : -1;
    osal_mutex_unlock(w->lock);
    return result; /* send completes its writes synchronously. */
}
void hal_ws_free(hal_ws_t *w) {
    if (!w) return;
    __atomic_store_n(&w->stop, true, __ATOMIC_RELEASE);
    osal_sem_take(w->done, OSAL_WAIT_FOREVER);
    esp_transport_destroy(w->transport);
    esp_transport_destroy(w->parent);
    osal_sem_delete(w->done);
    osal_mutex_delete(w->lock);
    osal_free(w);
}
