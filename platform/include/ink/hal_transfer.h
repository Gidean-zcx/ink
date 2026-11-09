/* Single-client byte stream for local HTTP uploads. Owner: exec.bg only. */
#ifndef INK_HAL_TRANSFER_H
#define INK_HAL_TRANSFER_H
#include <stddef.h>
#include <stdint.h>
int hal_transfer_listen(uint16_t *port); /* 0 selects a free port; -1 on failure. */
int hal_transfer_host(char *out, size_t cap); /* Listener IP; 0 on success, -1 if unavailable. */
int hal_transfer_accept(void); /* 1 accepted, 0 pending, -1 failed. */
int hal_transfer_read(void *data, size_t cap); /* >0 bytes, 0 pending, -1 disconnected. */
int hal_transfer_write(const void *data, size_t len); /* 0 if all sent. */
void hal_transfer_close(int listener); /* Also close the listener when nonzero. */
#endif
