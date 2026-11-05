/* Host transport lifetime, including callbacks queued past connection closure. */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>

static void ink_test_free(void *ptr);
static int ink_test_create(pthread_t *thread, const pthread_attr_t *attr, void *(*fn)(void *), void *arg);
#define free ink_test_free
#define pthread_create ink_test_create
#ifndef INK_TEST_BRIDGE_SOURCE
#define INK_TEST_BRIDGE_SOURCE "../../simulator/environment/hub_bridge.c"
#endif
#include INK_TEST_BRIDGE_SOURCE
#undef free
#undef pthread_create

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); exit(1); } } while (0)

static pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_cond = PTHREAD_COND_INITIALIZER;
static void *allocations[64];
static unsigned live_allocations, events[5], valid_events, stale_events;
static bool release_closed, fail_create;
static struct { vtk_event_fn fn; void *arg; } callbacks[8];
static unsigned callback_count;
typedef struct { uint64_t generation; bool closed; } owner_t;

static void *track(void *ptr) {
    CHECK(ptr != NULL);
    pthread_mutex_lock(&test_mutex);
    unsigned slot = 0;
    while (slot < 64 && allocations[slot]) slot++;
    CHECK(slot < 64);
    allocations[slot] = ptr;
    live_allocations++;
    pthread_mutex_unlock(&test_mutex);
    return ptr;
}
void *sim_host_malloc(size_t n) { return track(malloc(n)); }
void *sim_host_calloc(size_t n, size_t size) { return track(calloc(n, size)); }
void sim_host_free(void *ptr) { ink_test_free(ptr); }
static void ink_test_free(void *ptr) {
    pthread_mutex_lock(&test_mutex);
    for (unsigned i = 0; i < 64; i++) {
        if (ptr && allocations[i] == ptr) { allocations[i] = NULL; live_allocations--; break; }
    }
    free(ptr);
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_mutex);
}
static int ink_test_create(pthread_t *thread, const pthread_attr_t *attr, void *(*fn)(void *), void *arg) {
    return fail_create ? EAGAIN : pthread_create(thread, attr, fn, arg);
}
void vtk_host_call(void (*fn)(void *), void *arg) { fn(arg); }
void vtk_inject_external(vtk_event_fn fn, void *arg) {
    injected_t *e = arg;
    pthread_mutex_lock(&test_mutex);
    CHECK(callback_count < 8);
    callbacks[callback_count].fn = fn;
    callbacks[callback_count++].arg = arg;
    events[e->event]++;
    pthread_cond_broadcast(&test_cond);
    while (e->event == SIM_BRIDGE_CLOSED && !release_closed)
        CHECK(pthread_cond_wait(&test_cond, &test_mutex) == 0);
    pthread_mutex_unlock(&test_mutex);
}
void sim_net_bridge_event(void *owner, uint64_t generation, int event, const void *data, size_t len) {
    owner_t *o = owner;
    (void)event; (void)data; (void)len;
    if (o->closed || o->generation != generation) stale_events++;
    else valid_events++;
}

static void await(unsigned *value, unsigned expected) {
    struct timespec deadline;
    CHECK(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
    deadline.tv_sec += 3;
    pthread_mutex_lock(&test_mutex);
    while (*value != expected) {
        int rc = pthread_cond_timedwait(&test_cond, &test_mutex, &deadline);
        if (rc) fprintf(stderr, "wait: actual=%u expected=%u error=%d\n", *value, expected, rc);
        CHECK(rc == 0);
    }
    pthread_mutex_unlock(&test_mutex);
}
static void deliver(void) {
    for (unsigned i = 0; i < callback_count; i++) callbacks[i].fn(callbacks[i].arg);
    callback_count = 0;
}
static unsigned fd_count(void) {
    unsigned count = 0;
    for (int fd = 0; fd < 64; fd++) if (fcntl(fd, F_GETFD) >= 0) count++;
    return count;
}
static int endpoint(bool listen_on_port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(fd >= 0);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    CHECK(bind(fd, (struct sockaddr *)&addr, sizeof addr) == 0);
    socklen_t size = sizeof addr;
    CHECK(getsockname(fd, (struct sockaddr *)&addr, &size) == 0);
    if (listen_on_port) CHECK(listen(fd, 1) == 0);
    char url[96];
    snprintf(url, sizeof url, "ws://127.0.0.1:%u/v1/device", ntohs(addr.sin_port));
    CHECK(sim_hub_bridge_configure(url) == 0);
    return fd;
}
static void send_all(int fd, const void *data, size_t len) {
    const uint8_t *p = data;
    while (len) {
        ssize_t n = send(fd, p, len, 0);
        if (n < 0 && errno == EINTR) continue;
        CHECK(n > 0);
        p += n; len -= (size_t)n;
    }
}
static void *server(void *arg) {
    int fd = accept(*(int *)arg, NULL, NULL);
    CHECK(fd >= 0);
    char request[1024] = "", response[256], key[25], accepted[29];
    size_t used = 0;
    while (!strstr(request, "\r\n\r\n")) {
        CHECK(used + 1 < sizeof request);
        ssize_t n = recv(fd, request + used, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        CHECK(n == 1);
        request[++used] = 0;
    }
    const char *header = strstr(request, "Sec-WebSocket-Key: ");
    CHECK(header != NULL);
    memcpy(key, header + strlen("Sec-WebSocket-Key: "), 24); key[24] = 0;
    accept_key(key, accepted);
    int n = snprintf(response, sizeof response, "HTTP/1.1 101 Switching Protocols\r\nSec-WebSocket-Accept: %s\r\n\r\n", accepted);
    CHECK(n > 0 && (size_t)n < sizeof response);
    send_all(fd, response, (size_t)n);
    const uint8_t frame[] = {0x81, 5, 'S', 'Y', 'N', 'T', 'H'};
    send_all(fd, frame, sizeof frame);
    for (;;) {
        ssize_t received = recv(fd, request, sizeof request, 0);
        if (received < 0 && errno == EINTR) continue;
        CHECK(received >= 0 || errno == ECONNRESET);
        if (received <= 0) break;
    }
    CHECK(close(fd) == 0);
    return NULL;
}

int main(void) {
    struct rlimit limit;
    CHECK(getrlimit(RLIMIT_NOFILE, &limit) == 0);
    CHECK(limit.rlim_max >= 64);
    limit.rlim_cur = 64;
    CHECK(setrlimit(RLIMIT_NOFILE, &limit) == 0);
    unsigned initial_fds = fd_count(), current_rounds = 0, expected_stale = 0;
    owner_t owner = {0};
    for (unsigned i = 0; i < 80; i++) {
        bool connected = i % 10 != 9;
        bool current = connected && i % 2 == 0;
        int listener = endpoint(connected);
        pthread_t peer;
        memset(events, 0, sizeof events);
        release_closed = false;
        owner.generation++; owner.closed = false;
        if (connected) CHECK(pthread_create(&peer, NULL, server, &listener) == 0);
        sim_hub_bridge_t *b = sim_hub_bridge_open(&owner, owner.generation);
        CHECK(b != NULL);
        if (connected) {
            await(&events[SIM_BRIDGE_TEXT], 1);
            if (current) {
                deliver();
                CHECK(valid_events == ++current_rounds * 2);
            }
        } else await(&events[SIM_BRIDGE_CLOSED], 1);
        CHECK(sim_hub_bridge_send(b, "SYNTH_PENDING", 13, false));
        owner.closed = true; owner.generation++;
        sim_hub_bridge_close(b);
        await(&events[SIM_BRIDGE_CLOSED], 1);
        /* close must return while the worker is held at its final injection. */
        pthread_mutex_lock(&test_mutex);
        release_closed = true;
        pthread_cond_broadcast(&test_cond);
        pthread_mutex_unlock(&test_mutex);
        await(&live_allocations, 0);
        if (connected) CHECK(pthread_join(peer, NULL) == 0);
        CHECK(close(listener) == 0);
        CHECK(fd_count() == initial_fds);
        deliver();
        expected_stale += connected && !current ? 3 : 1;
        CHECK(stale_events == expected_stale);
    }
    int reserved = endpoint(false);
    unsigned reserved_fds = fd_count();
    fail_create = true;
    CHECK(sim_hub_bridge_open(&owner, owner.generation) == NULL);
    fail_create = false;
    CHECK(live_allocations == 0 && fd_count() == reserved_fds);
    int exhausted[64];
    unsigned count = 0;
    while (count < 64) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) { CHECK(errno == EMFILE); break; }
        exhausted[count++] = fd;
    }
    CHECK(sim_hub_bridge_open(&owner, owner.generation) == NULL);
    CHECK(live_allocations == 0);
    while (count) CHECK(close(exhausted[--count]) == 0);
    CHECK(fd_count() == reserved_fds);
    release_closed = true;
    CHECK(sim_hub_bridge_open(&owner, owner.generation) != NULL);
    sim_hub_bridge_shutdown();
    callback_count = 0; /* Shutdown discards callbacks after the kernel stops. */
    CHECK(live_allocations == 0 && fd_count() == reserved_fds);
    CHECK(close(reserved) == 0);
    printf("PASS: 80 reconnects, 2 startup failures, shutdown, %u stale callbacks; fds=%u\n", stale_events, fd_count());
    return 0;
}
