/* Single-client byte stream for local HTTP uploads. Owner: exec.bg only. */
#ifndef INK_HAL_TRANSFER_H
#define INK_HAL_TRANSFER_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native servers call this policy for every request and storage operation. */
enum { HAL_TRANSFER_OPEN, HAL_TRANSFER_READ, HAL_TRANSFER_WRITE, HAL_TRANSFER_SYNC,
    HAL_TRANSFER_CLOSE, HAL_TRANSFER_STAT, HAL_TRANSFER_DIR_OPEN, HAL_TRANSFER_DIR_NEXT,
    HAL_TRANSFER_DIR_CLOSE, HAL_TRANSFER_MKDIR, HAL_TRANSFER_REMOVE, HAL_TRANSFER_MOVE,
    HAL_TRANSFER_SPACE, HAL_TRANSFER_RECEIVED };
typedef struct {
    int op;
    const char *path, *to;
    void *handle, *data;
    size_t len;
    int64_t value;
    uint64_t free_bytes, total;
    char name[64];
    bool directory;
} hal_transfer_io_t;
typedef struct {
    bool (*authorize)(const char *code, const char *token, char issued[33]);
    int64_t (*file)(hal_transfer_io_t *io);
} hal_transfer_policy_t;
int hal_transfer_server_start(const hal_transfer_policy_t *policy, uint16_t *port); /* 1 native, 0 stream, -1 failed. */
void hal_transfer_server_stop(void); /* Join callbacks before returning. */
bool hal_transfer_hotspot(void);
/* Paint dark modules including a four-module quiet zone; UI task only. */
int hal_transfer_qr(const char *url, const char *code, void (*pixel)(void *, int, int, bool), void *ctx);
int hal_transfer_listen(uint16_t *port); /* 0 selects a free port; -1 on failure. */
int hal_transfer_host(char *out, size_t cap); /* Listener IP; 0 on success, -1 if unavailable. */
int hal_transfer_accept(void); /* 1 accepted, 0 pending, -1 failed. */
int hal_transfer_read(void *data, size_t cap); /* >0 bytes, 0 pending, -1 disconnected. */
int hal_transfer_write(const void *data, size_t len); /* 0 if all sent. */
void hal_transfer_close(int listener); /* Also close the listener when nonzero. */
#ifdef __cplusplus
}
#endif
#endif
