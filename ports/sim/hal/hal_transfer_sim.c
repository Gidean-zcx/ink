/* Host sockets stay outside simulated task stacks. No worker thread. */
#include "ink/hal_transfer.h"
#include "vtk.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

static int listener = -1, client = -1;
typedef struct { int op, result; uint16_t port; void *data; size_t len; } request_t;

static void on_host(void *arg) {
    request_t *r = arg;
    r->result = -1;
    if (r->op == 0) {
        struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(r->port), .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
        socklen_t n = sizeof addr;
        listener = socket(AF_INET, SOCK_STREAM, 0);
        if (listener < 0) return;
        if (bind(listener, (struct sockaddr *)&addr, sizeof addr) || listen(listener, 1) ||
            fcntl(listener, F_SETFL, O_NONBLOCK) || getsockname(listener, (struct sockaddr *)&addr, &n)) {
            close(listener); listener = -1; return;
        }
        r->port = ntohs(addr.sin_port); r->result = 0;
    } else if (r->op == 1) {
        client = accept(listener, NULL, NULL);
        if (client < 0) { if (errno == EAGAIN || errno == EWOULDBLOCK) r->result = 0; return; }
        if (fcntl(client, F_SETFL, O_NONBLOCK)) { close(client); client = -1; return; }
#ifdef SO_NOSIGPIPE
        int yes = 1;
        if (setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof yes)) { close(client); client = -1; return; }
#endif
        r->result = 1;
    } else if (r->op == 2) {
        ssize_t n = recv(client, r->data, r->len, 0);
        if (n > 0) r->result = (int)n;
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) r->result = 0;
    } else if (r->op == 3) {
        size_t done = 0;
        while (done < r->len) {
            ssize_t n = send(client, (char *)r->data + done, r->len - done,
#ifdef MSG_NOSIGNAL
                             MSG_NOSIGNAL
#else
                             0
#endif
            );
            if (n <= 0) return;
            done += (size_t)n;
        }
        r->result = 0;
    } else if (r->op == 6) {
        struct sockaddr_in addr;
        socklen_t n = sizeof addr;
        if (listener >= 0 && !getsockname(listener, (struct sockaddr *)&addr, &n) &&
            inet_ntop(AF_INET, &addr.sin_addr, r->data, r->len)) r->result = 0;
    } else {
        if (client >= 0) {
            char discard[512];
            shutdown(client, SHUT_WR);
            for (size_t drained = 0; drained < 65536;) {
                ssize_t n = recv(client, discard, sizeof discard, 0);
                if (n <= 0) break;
                drained += (size_t)n;
            }
            close(client);
        }
        client = -1;
        if (r->op == 5 && listener >= 0) { close(listener); listener = -1; }
        r->result = 0;
    }
}

int hal_transfer_listen(uint16_t *port) {
    request_t r = {.op = 0, .port = *port}; vtk_host_call(on_host, &r); *port = r.port; return r.result;
}
int hal_transfer_host(char *out, size_t cap) {
    if (cap) out[0] = 0;
    request_t r = {.op = 6, .data = out, .len = cap}; vtk_host_call(on_host, &r); return r.result;
}
int hal_transfer_accept(void) { request_t r = {.op = 1}; vtk_host_call(on_host, &r); return r.result; }
int hal_transfer_read(void *data, size_t cap) {
    request_t r = {.op = 2, .data = data, .len = cap}; vtk_host_call(on_host, &r); return r.result;
}
int hal_transfer_write(const void *data, size_t len) {
    request_t r = {.op = 3, .data = (void *)data, .len = len}; vtk_host_call(on_host, &r); return r.result;
}
void hal_transfer_close(int all) { request_t r = {.op = all ? 5 : 4}; vtk_host_call(on_host, &r); }
