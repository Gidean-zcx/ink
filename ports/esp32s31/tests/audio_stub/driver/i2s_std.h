#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_TIMEOUT -1
#define ESP_FAIL -2
#define I2S_NUM_0 0
#define I2S_ROLE_SLAVE 1
#define I2S_GPIO_UNUSED -1
#define I2S_DATA_BIT_WIDTH_32BIT 32
#define I2S_SLOT_MODE_STEREO 2
typedef struct fake_i2s *i2s_chan_handle_t;
typedef struct { int id, role, dma_desc_num, dma_frame_num; bool auto_clear_after_cb; } i2s_chan_config_t;
typedef struct {
    struct { int sample_rate_hz; } clk_cfg;
    struct { int data_bit_width, slot_mode; } slot_cfg;
    struct { int mclk, bclk, ws, dout, din; } gpio_cfg;
} i2s_std_config_t;
typedef struct { size_t size; } i2s_event_data_t;
typedef bool (*i2s_isr_callback_t)(i2s_chan_handle_t, i2s_event_data_t *, void *);
typedef struct { i2s_isr_callback_t on_recv_q_ovf; } i2s_event_callbacks_t;
#define I2S_CHANNEL_DEFAULT_CONFIG(i, r) {.id = i, .role = r}
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) {.sample_rate_hz = rate}
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bits, mode) {.data_bit_width = bits, .slot_mode = mode}
esp_err_t i2s_new_channel(const i2s_chan_config_t *, i2s_chan_handle_t *, i2s_chan_handle_t *);
esp_err_t i2s_del_channel(i2s_chan_handle_t);
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t, const i2s_std_config_t *);
esp_err_t i2s_channel_register_event_callback(i2s_chan_handle_t, const i2s_event_callbacks_t *, void *);
esp_err_t i2s_channel_enable(i2s_chan_handle_t);
esp_err_t i2s_channel_disable(i2s_chan_handle_t);
esp_err_t i2s_channel_preload_data(i2s_chan_handle_t, const void *, size_t, size_t *);
esp_err_t i2s_channel_write(i2s_chan_handle_t, const void *, size_t, size_t *, uint32_t);
esp_err_t i2s_channel_read(i2s_chan_handle_t, void *, size_t, size_t *, uint32_t);
