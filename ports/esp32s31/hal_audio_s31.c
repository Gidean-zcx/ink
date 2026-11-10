/* BH1098G / NS4150, factory @690a023: metalio_audio.cc and bt_audio_codec.cc.
 * Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
 * Copyright (c) 2025 Project Contributors. MIT: vendor/s31_sleep/LICENSE. */
#include <limits.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "bh1098g/bh1098g.h"
#include "ink/hal_audio.h"
#include "ink/hal_power.h"
#include "ink/osal.h"
#include "metalio_plus/metalio_board.h"

#define INK_AUDIO_CHUNK 240
#define INK_AUDIO_DMA 4
#define INK_AUDIO_RUNWAY_US (INK_AUDIO_CHUNK * INK_AUDIO_DMA * 1000000LL / HAL_AUDIO_RATE)
static struct {
    osal_mutex_t *lock;
    osal_sem_t *done;
    i2s_chan_handle_t tx, rx;
    bool playing, capturing;
    unsigned epoch, lost;
    int64_t tail;
    int32_t *out, *in;
    int16_t *mic, *ref;
    hal_audio_capture_cb cb;
    void *ctx;
} A;

void ink_s31_audio_init(void) {
    A.lock = osal_mutex_create();
    A.done = osal_sem_create(0, 1);
    A.epoch = UINT_MAX;
    if (!A.lock || !A.done) osal_panic("audio HAL: synchronization setup failed");
}
static bool ink_amp(bool on) { return metalio_board_set(0, MB_P0_AMP, on ? MB_P0_AMP : 0) == HAL_I2C_OK; }
static void ink_audio_idle(void) {
    if (A.playing || A.capturing) return;
    if (A.tx) i2s_del_channel(A.tx);
    if (A.rx) i2s_del_channel(A.rx);
    A.tx = A.rx = NULL;
    osal_free(A.out);
    osal_free(A.in);
    osal_free(A.mic);
    A.out = A.in = NULL;
    A.mic = A.ref = NULL;
}
static bool ink_rx_lost(i2s_chan_handle_t chan, i2s_event_data_t *ev, void *ctx) {
    (void)chan;
    (void)ctx;
    __atomic_add_fetch(&A.lost, ev->size / 8, __ATOMIC_RELAXED);
    return false;
}
static bool ink_audio_ready(void) { /* lifecycle lock held; never reset the shared modem rail */
    if (!metalio_board_ready() || metalio_board_rails_cut() || !(metalio_board_output(1) & MB_P1_SHARED_PWR)) return false;
    unsigned epoch = metalio_board_shared_power_epoch();
    if (A.epoch != epoch) {
        if (!ink_amp(false)) return false;
        uint64_t ready = metalio_board_shared_power_since_us() + BH1098G_BOOT_US;
        uint64_t now = osal_now_us();
        if (now < ready) osal_task_delay_us(ready - now);
        hal_uart_t *u = hal_uart_open(BH1098G_UART_PORT, BH1098G_BAUD);
        if (!u || bh1098g_set_mode(u, 1)) return false;
        char line[128];
        uint64_t deadline = osal_now_us() + 1000000;
        bool ack = false;
        while (osal_now_us() < deadline) {
            if (hal_uart_read_line(u, line, sizeof line, 10) < 0) continue;
            if (strstr(line, "ERROR") || strstr(line, "FAIL")) return false;
            const char *mode = strstr(line, "SET MODE 1");
            if (mode && (mode[10] == 0 || mode[10] == ' ')) { ack = true; break; }
        }
        if (!ack) return false;
        A.epoch = epoch;
    }
    if (A.tx) return true;
    A.out = osal_malloc(INK_AUDIO_CHUNK * 8, OSAL_MEM_EXTERNAL);
    A.in = osal_malloc(INK_AUDIO_CHUNK * 8, OSAL_MEM_EXTERNAL);
    A.mic = osal_malloc(INK_AUDIO_CHUNK * 4, OSAL_MEM_EXTERNAL);
    if (!A.out || !A.in || !A.mic) goto fail;
    A.ref = A.mic + INK_AUDIO_CHUNK;
    i2s_chan_config_t c = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    c.dma_desc_num = INK_AUDIO_DMA;
    c.dma_frame_num = INK_AUDIO_CHUNK;
    c.auto_clear_after_cb = true;
    i2s_std_config_t s = {
        .clk_cfg = {.sample_rate_hz = HAL_AUDIO_RATE, .clk_src = I2S_CLK_SRC_DEFAULT,
                    .mclk_multiple = I2S_MCLK_MULTIPLE_256},
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = 42, .ws = 43, .dout = 44, .din = 45},
    };
    i2s_event_callbacks_t cb = {.on_recv_q_ovf = ink_rx_lost};
    if (i2s_new_channel(&c, &A.tx, &A.rx) != ESP_OK || i2s_channel_init_std_mode(A.tx, &s) != ESP_OK ||
        i2s_channel_init_std_mode(A.rx, &s) != ESP_OK || i2s_channel_register_event_callback(A.rx, &cb, NULL) != ESP_OK) goto fail;
    return true;
fail:
    ink_audio_idle();
    return false;
}
int hal_audio_route_set(hal_audio_route_t route) { return route == HAL_ROUTE_SPEAKER ? 0 : -1; }
static bool ink_play_enable(void) {
    memset(A.out, 0, INK_AUDIO_CHUNK * 8);
    for (int i = 0; i < INK_AUDIO_DMA; i++) {
        size_t bytes = 0;
        if (i2s_channel_preload_data(A.tx, A.out, INK_AUDIO_CHUNK * 8, &bytes) != ESP_OK || bytes != INK_AUDIO_CHUNK * 8) return false;
    }
    return i2s_channel_enable(A.tx) == ESP_OK;
}
void hal_audio_get_info(hal_audio_info_t *out) {
    *out = (hal_audio_info_t){.has_speaker = A.lock != NULL, .has_mic = A.lock != NULL, .mic_reference = true,
                             .chunk_samples = INK_AUDIO_CHUNK, .runway_ms = INK_AUDIO_RUNWAY_US / 1000};
}
int hal_audio_play_start(void) {
    if (!A.lock) return -1;
    osal_mutex_lock(A.lock, OSAL_WAIT_FOREVER);
    int r = -1;
    if (!A.playing) {
        hal_power_request(HAL_PWR_AUDIO_OUT, true);
        if (ink_audio_ready() && ink_play_enable()) { A.playing = true; A.tail = 0; r = 0; }
        else { ink_audio_idle(); hal_power_request(HAL_PWR_AUDIO_OUT, false); }
    }
    osal_mutex_unlock(A.lock);
    return r;
}
int hal_audio_play_write(const int16_t *pcm, int n, int64_t timeout_us) {
    if (!A.lock || !pcm || n < 0) return -1;
    if (!n) return 0;
    osal_mutex_lock(A.lock, OSAL_WAIT_FOREVER);
    int count = 0;
    int64_t deadline = (int64_t)osal_now_us() + (timeout_us > 0 ? timeout_us : 0);
    if (!A.playing || !ink_amp(true)) count = -1;
    while (count >= 0 && count < n) {
        int chunk = n - count < INK_AUDIO_CHUNK ? n - count : INK_AUDIO_CHUNK;
        for (int i = 0; i < chunk; i++) A.out[i * 2] = A.out[i * 2 + 1] = (int32_t)pcm[count + i] * 65536;
        int64_t left = deadline - (int64_t)osal_now_us();
        size_t bytes = 0;
        esp_err_t rc = i2s_channel_write(A.tx, A.out, (size_t)chunk * 8, &bytes,
                                       timeout_us < 0 ? UINT32_MAX : left > 0 ? (uint32_t)(left / 1000) : 0);
        count += (int)(bytes / 8);
        if (bytes) A.tail = (int64_t)osal_now_us() + INK_AUDIO_RUNWAY_US;
        if (rc != ESP_OK || bytes < (size_t)chunk * 8) break;
    }
    if (count == 0 && A.tail <= (int64_t)osal_now_us()) ink_amp(false);
    osal_mutex_unlock(A.lock);
    return count;
}
int64_t hal_audio_play_delay_us(void) { /* conservative DMA bound, not an acoustic measurement */
    if (!A.lock) return 0;
    osal_mutex_lock(A.lock, OSAL_WAIT_FOREVER);
    int64_t left = A.playing ? A.tail - (int64_t)osal_now_us() : 0;
    osal_mutex_unlock(A.lock);
    return left > 0 ? left : 0;
}
void hal_audio_play_stop(bool drain) {
    if (!A.lock) return;
    osal_mutex_lock(A.lock, OSAL_WAIT_FOREVER);
    if (A.playing) {
        int64_t left = A.tail - (int64_t)osal_now_us();
        if (drain && left > 0) osal_task_delay_us((uint64_t)left);
        i2s_channel_disable(A.tx);
        ink_amp(false);
        A.playing = false;
        ink_audio_idle();
        hal_power_request(HAL_PWR_AUDIO_OUT, false);
    }
    osal_mutex_unlock(A.lock);
}
static int16_t ink_sample(int32_t word) {
    int32_t v = word >> 12;
    return v > INT16_MAX ? INT16_MAX : v < -INT16_MAX ? -INT16_MAX : (int16_t)v;
}
static void ink_capture(void *arg) {
    (void)arg;
    while (__atomic_load_n(&A.capturing, __ATOMIC_ACQUIRE)) {
        size_t bytes = 0;
        esp_err_t rc = i2s_channel_read(A.rx, A.in, INK_AUDIO_CHUNK * 8, &bytes, 250);
        int n = (int)(bytes / 8);
        if (rc != ESP_OK && !n) continue;
        if (!n || !__atomic_load_n(&A.capturing, __ATOMIC_ACQUIRE)) continue;
        for (int i = 0; i < n; i++) { A.mic[i] = ink_sample(A.in[i * 2]); A.ref[i] = ink_sample(A.in[i * 2 + 1]); }
        uint64_t now = osal_now_us(), span = (uint64_t)n * 1000000 / HAL_AUDIO_RATE;
        A.cb(A.ctx, A.mic, A.ref, n, (int)__atomic_exchange_n(&A.lost, 0, __ATOMIC_RELAXED), now > span ? now - span : 0);
    }
    osal_sem_give(A.done);
}
int hal_audio_capture_start(hal_audio_capture_cb cb, void *ctx) {
    if (!A.lock || !cb) return -1;
    osal_mutex_lock(A.lock, OSAL_WAIT_FOREVER);
    int r = -1;
    if (!A.capturing) {
        hal_power_request(HAL_PWR_AUDIO_IN, true);
        if (ink_audio_ready() && i2s_channel_enable(A.rx) == ESP_OK) {
            A.cb = cb;
            A.ctx = ctx;
            __atomic_store_n(&A.lost, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&A.capturing, true, __ATOMIC_RELEASE);
            osal_task_cfg_t task = {.name = "audio-capture", .fn = ink_capture, .stack_bytes = 4096,
                .priority = OSAL_PRIO_HIGH, .core = 0, .flags = OSAL_TASK_STACK_EXTERNAL};
            if (osal_task_create(&task, NULL) == OSAL_OK) r = 0;
            else { __atomic_store_n(&A.capturing, false, __ATOMIC_RELEASE); i2s_channel_disable(A.rx); }
        }
        if (r) { ink_audio_idle(); hal_power_request(HAL_PWR_AUDIO_IN, false); }
    }
    osal_mutex_unlock(A.lock);
    return r;
}
void hal_audio_capture_stop(void) {
    if (!A.lock) return;
    osal_mutex_lock(A.lock, OSAL_WAIT_FOREVER);
    if (__atomic_exchange_n(&A.capturing, false, __ATOMIC_ACQ_REL)) {
        osal_sem_take(A.done, OSAL_WAIT_FOREVER);
        i2s_channel_disable(A.rx);
        ink_audio_idle();
        hal_power_request(HAL_PWR_AUDIO_IN, false);
    }
    osal_mutex_unlock(A.lock);
}
