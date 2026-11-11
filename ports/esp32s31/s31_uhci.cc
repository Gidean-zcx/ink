/* Thin C bridge to the unmodified vendor controller; no additional task. */
#include "s31_uhci.h"
#include "uart_uhci.h"
#include "freertos/queue.h"

static UartUhci ink_uhci;
static QueueHandle_t ink_rx_queue;
static std::atomic<bool> ink_overflow{false};
static bool IRAM_ATTR ink_rx(const UartUhci::RxEventData& event, void *) {
    BaseType_t wake = pdFALSE;
    xQueueSendFromISR(ink_rx_queue, &event.buffer, &wake);
    return wake == pdTRUE;
}
static bool IRAM_ATTR ink_lost(void *) {
    ink_overflow.store(true);
    return false;
}
bool ink_s31_uhci_init(void) {
    /* Four descriptors can deliver at most four outstanding buffer pointers. */
    ink_rx_queue = xQueueCreate(4, sizeof(UartUhci::RxBuffer *));
    if (!ink_rx_queue) return false;
    UartUhci::Config config = {.uart_port = UART_NUM_2, .dma_burst_size = 32, .rx_pool = {4, 1600}};
    if (ink_uhci.Init(config) != ESP_OK) {
        vQueueDelete(ink_rx_queue);
        return false;
    }
    ink_uhci.SetRxCallback(ink_rx, nullptr);
    ink_uhci.SetOverflowCallback(ink_lost, nullptr);
    return true;
}
bool ink_s31_uhci_active(bool active) {
    if (!active) return ink_uhci.StopReceive() == ESP_OK;
    if (ink_uhci.IsReceiving()) return true;
    ink_overflow.store(false);
    return ink_uhci.StartReceive() == ESP_OK;
}
bool ink_s31_uhci_take(ink_s31_uhci_buffer_t *out) {
    UartUhci::RxBuffer *buffer;
    if (xQueueReceive(ink_rx_queue, &buffer, 0) != pdTRUE) return false;
    *out = {buffer->data, buffer->size, buffer};
    return true;
}
void ink_s31_uhci_return(void *token) { ink_uhci.ReturnBuffer(static_cast<UartUhci::RxBuffer *>(token)); }
bool ink_s31_uhci_recovered(void) {
    /* ReturnBuffer clears HasOverflow only after all pre-gap buffers are returned. */
    return !ink_uhci.HasOverflow() && ink_overflow.exchange(false);
}
bool ink_s31_uhci_transmit(const uint8_t *data, size_t len) { return ink_uhci.Transmit(data, len) == ESP_OK; }
