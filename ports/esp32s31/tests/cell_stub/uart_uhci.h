#pragma once
#include "cell_idf.h"
#include <atomic>
class UartUhci {
public:
    struct RxBuffer { uint8_t* data; size_t capacity, size; uint32_t index; };
    struct RxEventData { RxBuffer* buffer; size_t recv_size; };
    using RxCallback = bool(*)(const RxEventData&, void*);
    using OverflowCallback = bool(*)(void*);
    struct BufferPoolConfig { size_t buffer_count, buffer_size; };
    struct Config { uart_port_t uart_port; size_t dma_burst_size; BufferPoolConfig rx_pool; };
    std::atomic<bool> running{false}, overflow{false};
    RxCallback callback = nullptr; void *context = nullptr;
    OverflowCallback lost = nullptr; void *lost_context = nullptr;
    esp_err_t Init(const Config&);
    void Deinit();
    void SetRxCallback(RxCallback fn, void *p) { callback = fn; context = p; }
    void SetOverflowCallback(OverflowCallback fn, void *p) { lost=fn; lost_context=p; }
    esp_err_t StartReceive();
    esp_err_t StopReceive();
    void ReturnBuffer(RxBuffer*);
    bool IsReceiving() const { return running; }
    bool HasOverflow() const { return overflow; }
    bool CheckAndClearOverflow() { return overflow.exchange(false); }
    esp_err_t Transmit(const uint8_t*, size_t);
};
