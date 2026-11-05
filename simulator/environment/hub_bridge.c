/* Loopback development transport: all socket operations live on a host thread. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "hub_bridge.h"
#include "sim.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define BRIDGE_LIMIT (256u * 1024u)
#define FRAME_LIMIT 32768u

typedef struct message {
    struct message *next;
    size_t len;
    bool binary;
    uint8_t data[];
} message_t;

struct sim_hub_bridge {
    struct sim_hub_bridge *next;
    pthread_t thread;
    pthread_mutex_t mutex;
    int wake[2];
    int socket;
    atomic_bool stop;
    bool started;
    void *owner;
    uint64_t generation;
    message_t *head, *tail;
    size_t queued;
};

typedef struct injected {
    struct injected *next;
    void *owner;
    uint64_t generation;
    int event;
    size_t len;
    uint8_t data[];
} injected_t;

static sim_hub_bridge_t *g_bridges;
static unsigned g_port;
static injected_t *g_injected;
static pthread_mutex_t g_injected_mutex = PTHREAD_MUTEX_INITIALIZER;

int sim_hub_bridge_configure(const char *url) {
    unsigned port = 0;
    int used = 0;
    if (!url || sscanf(url, "ws://127.0.0.1:%u/v1/device%n", &port, &used) != 1 || !used || url[used] || !port || port > 65535)
        return -1;
    g_port = port;
    return 0;
}

static void injected_cb(void *arg) {
    injected_t *e = arg;
    pthread_mutex_lock(&g_injected_mutex);
    injected_t **p = &g_injected;
    while (*p && *p != e) p = &(*p)->next;
    if (*p) *p = e->next;
    pthread_mutex_unlock(&g_injected_mutex);
    sim_net_bridge_event(e->owner, e->generation, e->event, e->data, e->len);
    sim_host_free(e);
}

static void inject(sim_hub_bridge_t *b, int event, const void *data, size_t len) {
    injected_t *e = malloc(sizeof *e + len);
    if (!e) abort();
    *e = (injected_t){.owner = b->owner, .generation = b->generation, .event = event, .len = len};
    if (len) memcpy(e->data, data, len);
    pthread_mutex_lock(&g_injected_mutex);
    e->next = g_injected;
    g_injected = e;
    pthread_mutex_unlock(&g_injected_mutex);
    vtk_inject_external(injected_cb, e);
}

static int ready(sim_hub_bridge_t *b, short events) {
    struct pollfd fds[2] = {{b->socket, events, 0}, {b->wake[0], POLLIN, 0}};
    for (;;) {
        if (atomic_load(&b->stop)) return -1;
        int n = poll(fds, 2, 2000);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        if (fds[1].revents) {
            uint8_t bytes[128];
            while (read(b->wake[0], bytes, sizeof bytes) > 0) {}
            if (atomic_load(&b->stop)) return -1;
        }
        if (fds[0].revents & events) return 0;
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) return -1;
    }
}

static int transfer(sim_hub_bridge_t *b, void *buffer, size_t len, bool sending) {
    uint8_t *p = buffer;
    while (len) {
        ssize_t n = sending ? send(b->socket, p, len, MSG_NOSIGNAL) : recv(b->socket, p, len, 0);
        if (n > 0) { p += n; len -= (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && !ready(b, sending ? POLLOUT : POLLIN)) continue;
        return -1;
    }
    return 0;
}

static void base64(const uint8_t *data, size_t len, char *out) {
    static const char abc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t pos = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)data[i] << 16;
        if (i + 1 < len) v |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) v |= data[i + 2];
        out[pos++] = abc[v >> 18]; out[pos++] = abc[v >> 12 & 63];
        out[pos++] = i + 1 < len ? abc[v >> 6 & 63] : '=';
        out[pos++] = i + 2 < len ? abc[v & 63] : '=';
    }
    out[pos] = 0;
}

static uint32_t rotate(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

/* RFC 6455's SHA-1 challenge, always 60 input bytes (24-byte key + GUID). */
static void accept_key(const char *key, char out[29]) {
    uint8_t bytes[128] = {0}, digest_bytes[20];
    memcpy(bytes, key, 24);
    memcpy(bytes + 24, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 36);
    bytes[60] = 0x80; bytes[126] = 1; bytes[127] = 0xe0;
    uint32_t h[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    for (int block = 0; block < 2; block++) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            const uint8_t *p = bytes + block * 64 + i * 4;
            w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
        }
        for (int i = 16; i < 80; i++) w[i] = rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f = i < 20 ? (b & c) | (~b & d) : i < 40 ? b ^ c ^ d : i < 60 ? (b & c) | (b & d) | (c & d) : b ^ c ^ d;
            uint32_t k = i < 20 ? 0x5a827999 : i < 40 ? 0x6ed9eba1 : i < 60 ? 0x8f1bbcdc : 0xca62c1d6;
            uint32_t t = rotate(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotate(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    for (int i = 0; i < 20; i++) digest_bytes[i] = (uint8_t)(h[i / 4] >> (24 - (i % 4) * 8));
    base64(digest_bytes, 20, out);
}

static int handshake(sim_hub_bridge_t *b) {
    b->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (b->socket < 0) return -1;
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(b->socket, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    fcntl(b->socket, F_SETFL, O_NONBLOCK);
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons((uint16_t)g_port)};
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(b->socket, (struct sockaddr *)&address, sizeof address) && (errno != EINPROGRESS || ready(b, POLLOUT))) return -1;
    int error = 0;
    socklen_t size = sizeof error;
    if (getsockopt(b->socket, SOL_SOCKET, SO_ERROR, &error, &size) || error) return -1;
    uint8_t nonce[16];
    char key[25], expected[29], request[512], reply[2048];
    arc4random_buf(nonce, sizeof nonce);
    base64(nonce, sizeof nonce, key);
    accept_key(key, expected);
    int n = snprintf(request, sizeof request,
                     "GET /v1/device HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                     "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n", g_port, key);
    if (transfer(b, request, (size_t)n, true)) return -1;
    size_t used = 0;
    do {
        if (used + 1 >= sizeof reply || transfer(b, reply + used, 1, false)) return -1;
        reply[++used] = 0;
    } while (!strstr(reply, "\r\n\r\n"));
    if (strncmp(reply, "HTTP/1.1 101 ", 13)) return -1;
    const char *accept = strstr(reply, "Sec-WebSocket-Accept: ");
    return !accept || strncmp(accept + strlen("Sec-WebSocket-Accept: "), expected, 28) ? -1 : 0;
}

static int send_frame(sim_hub_bridge_t *b, int opcode, const uint8_t *data, size_t len) {
    uint8_t header[8], mask[4];
    if (len > FRAME_LIMIT) return -1;
    arc4random_buf(mask, sizeof mask);
    size_t h = 2;
    header[0] = (uint8_t)(0x80 | opcode);
    header[1] = (uint8_t)(0x80 | (len < 126 ? len : 126));
    if (len >= 126) { header[2] = (uint8_t)(len >> 8); header[3] = (uint8_t)len; h = 4; }
    memcpy(header + h, mask, 4); h += 4;
    uint8_t *masked = malloc(len ? len : 1);
    if (!masked) return -1;
    for (size_t i = 0; i < len; i++) masked[i] = data[i] ^ mask[i % 4];
    int result = transfer(b, header, h, true) || transfer(b, masked, len, true);
    free(masked);
    return result ? -1 : 0;
}

static void *network_thread(void *arg) {
    sim_hub_bridge_t *b = arg;
    if (handshake(b)) goto done;
    inject(b, SIM_BRIDGE_OPEN, NULL, 0);
    uint8_t *buffer = malloc(FRAME_LIMIT);
    size_t used = 0;
    int fragmented = 0;
    if (!buffer) goto done;
    while (!atomic_load(&b->stop)) {
        pthread_mutex_lock(&b->mutex);
        message_t *m = b->head;
        if (m) { b->head = m->next; b->queued -= m->len; if (!b->head) b->tail = NULL; }
        pthread_mutex_unlock(&b->mutex);
        if (m) {
            int result = send_frame(b, m->binary ? 2 : 1, m->data, m->len);
            free(m);
            if (result) break;
            continue;
        }
        struct pollfd fds[2] = {{b->socket, POLLIN, 0}, {b->wake[0], POLLIN, 0}};
        if (poll(fds, 2, -1) < 0) { if (errno == EINTR) continue; break; }
        if (fds[1].revents) {
            uint8_t bytes[128];
            while (read(b->wake[0], bytes, sizeof bytes) > 0) {}
        }
        if (!(fds[0].revents & POLLIN)) {
            if (fds[0].revents) break;
            continue;
        }
        uint8_t head[2];
        if (transfer(b, head, 2, false) || (head[0] & 0x70) || (head[1] & 0x80)) break;
        int opcode = head[0] & 15;
        size_t len = head[1] & 127;
        if (len == 127) break; /* bounded development frames never require 64-bit lengths */
        if (len == 126) {
            uint8_t ext[2];
            if (transfer(b, ext, 2, false)) break;
            len = (size_t)ext[0] << 8 | ext[1];
        }
        if (len > FRAME_LIMIT || used + len > FRAME_LIMIT) break;
        if (opcode >= 8) {
            uint8_t control[125];
            if (!(head[0] & 0x80) || len > sizeof control || transfer(b, control, len, false)) break;
            if (opcode == 8) { send_frame(b, 8, control, len); break; }
            if (opcode == 9 && send_frame(b, 10, control, len)) break;
            if (opcode != 9 && opcode != 10) break;
            continue;
        }
        if (opcode == 1 || opcode == 2) { if (fragmented) break; fragmented = opcode; }
        else if (opcode != 0 || !fragmented) break;
        if (transfer(b, buffer + used, len, false)) break;
        used += len;
        if (head[0] & 0x80) {
            inject(b, fragmented == 1 ? SIM_BRIDGE_TEXT : SIM_BRIDGE_BINARY, buffer, used);
            used = 0; fragmented = 0;
        }
    }
    free(buffer);
done:
    if (b->socket >= 0) close(b->socket);
    inject(b, SIM_BRIDGE_CLOSED, NULL, 0);
    return NULL;
}

static void start_on_host(void *arg) {
    sim_hub_bridge_t *b = arg;
    pthread_mutex_init(&b->mutex, NULL);
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, b->wake)) return;
    fcntl(b->wake[0], F_SETFL, O_NONBLOCK);
    fcntl(b->wake[1], F_SETFL, O_NONBLOCK);
    b->started = pthread_create(&b->thread, NULL, network_thread, b) == 0;
}

sim_hub_bridge_t *sim_hub_bridge_open(void *owner, uint64_t generation) {
    sim_hub_bridge_t *b = sim_host_calloc(1, sizeof *b);
    if (!b) return NULL;
    b->owner = owner; b->generation = generation; b->socket = -1;
    b->wake[0] = b->wake[1] = -1;
    b->next = g_bridges; g_bridges = b;
    vtk_host_call(start_on_host, b);
    return b->started ? b : NULL;
}

typedef struct { sim_hub_bridge_t *b; message_t *m; bool ok; } enqueue_t;
static void enqueue_on_host(void *arg) {
    enqueue_t *e = arg;
    sim_hub_bridge_t *b = e->b;
    pthread_mutex_lock(&b->mutex);
    if (!atomic_load(&b->stop) && b->queued + e->m->len <= BRIDGE_LIMIT) {
        if (b->tail) b->tail->next = e->m;
        else b->head = e->m;
        b->tail = e->m;
        b->queued += e->m->len;
        e->ok = true;
        uint8_t byte = 1;
        (void)write(b->wake[1], &byte, 1);
    }
    pthread_mutex_unlock(&b->mutex);
}

bool sim_hub_bridge_send(sim_hub_bridge_t *b, const void *data, size_t len, bool binary) {
    if (!b || len > FRAME_LIMIT) return false;
    message_t *m = sim_host_malloc(sizeof *m + len);
    if (!m) return false;
    *m = (message_t){.len = len, .binary = binary};
    memcpy(m->data, data, len);
    enqueue_t e = {b, m, false};
    vtk_host_call(enqueue_on_host, &e);
    if (!e.ok) sim_host_free(m);
    return e.ok;
}

static void stop_on_host(void *arg) {
    sim_hub_bridge_t *b = arg;
    atomic_store(&b->stop, true);
    uint8_t byte = 1;
    if (b->wake[1] >= 0) (void)write(b->wake[1], &byte, 1);
}

void sim_hub_bridge_close(sim_hub_bridge_t *b) { if (b) vtk_host_call(stop_on_host, b); }

void sim_hub_bridge_shutdown(void) {
    for (sim_hub_bridge_t *b = g_bridges; b; b = b->next) stop_on_host(b);
    while (g_bridges) {
        sim_hub_bridge_t *b = g_bridges;
        g_bridges = b->next;
        if (b->started) pthread_join(b->thread, NULL);
        if (b->wake[0] >= 0) close(b->wake[0]);
        if (b->wake[1] >= 0) close(b->wake[1]);
        while (b->head) { message_t *m = b->head; b->head = m->next; free(m); }
        pthread_mutex_destroy(&b->mutex);
        free(b);
    }
    /* Kernel has stopped; queued external callbacks will not run again. */
    while (g_injected) { injected_t *e = g_injected; g_injected = e->next; free(e); }
}
