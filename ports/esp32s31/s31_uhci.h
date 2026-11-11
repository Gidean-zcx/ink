#ifndef INK_S31_UHCI_H
#define INK_S31_UHCI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { const uint8_t *data; size_t len; void *token; } ink_s31_uhci_buffer_t;
bool ink_s31_uhci_init(void);
bool ink_s31_uhci_active(bool active);
bool ink_s31_uhci_receiving(void);
void ink_s31_uhci_flush(void);
bool ink_s31_uhci_take(ink_s31_uhci_buffer_t *buffer);
void ink_s31_uhci_return(void *token);
bool ink_s31_uhci_recovered(void);
bool ink_s31_uhci_transmit(const uint8_t *data, size_t len);
bool ink_s31_cell_rx(void (*feed)(const uint8_t *, size_t), void (*lost)(void));
#ifdef __cplusplus
}
#endif
#endif
