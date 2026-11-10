#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "ink/hal_audio.h"
#include "ink/hal_power.h"
#include "ink/osal.h"
#include "metalio_plus/metalio_board.h"
#include "bh1098g/bh1098g.h"

void ink_s31_audio_init(void);
struct osal_mutex { pthread_mutex_t mutex; };
struct osal_sem { pthread_mutex_t mutex; pthread_cond_t cond; bool signaled; };
struct fake_i2s { bool tx, enabled; int preloads; };
static struct fake_i2s tx, rx;
static struct osal_mutex mutex;
static struct osal_sem done;
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static pthread_t worker;
static osal_task_cfg_t worker_cfg;
static bool threaded, stopping, feed, overflow, amp, powered = true, cut, ack = true, task_fail, uart_fail;
static int step, fail_step, allocations, fail_alloc, holds[HAL_PWR_COUNT], callbacks, lost_samples, mode_calls;
static unsigned epoch;
static uint64_t now, powered_since, delayed;
static size_t write_limit = SIZE_MAX;
static char trace[1024];
static size_t trace_n;
static void event(char c) { assert(trace_n + 1 < sizeof trace); trace[trace_n++] = c; trace[trace_n] = 0; }
static bool fail(void) { return ++step == fail_step; }
static i2s_isr_callback_t lost_cb;

uint64_t osal_now_us(void) { return __atomic_load_n(&now, __ATOMIC_RELAXED); }
void osal_task_delay_us(uint64_t us) { delayed += us; __atomic_add_fetch(&now, us, __ATOMIC_RELAXED); event('w'); }
void *osal_malloc(size_t n, uint32_t caps) {
    assert(caps == OSAL_MEM_EXTERNAL);
    if (fail_alloc > 0 && --fail_alloc == 0) return NULL;
    void *p = malloc(n);
    assert(p);
    allocations++;
    return p;
}
void osal_free(void *p) { if (p) { allocations--; free(p); } }
osal_mutex_t *osal_mutex_create(void) { assert(!pthread_mutex_init(&mutex.mutex, NULL)); return &mutex; }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t timeout) {
    (void)timeout;
    assert(!pthread_mutex_lock(&m->mutex));
    return OSAL_OK;
}
void osal_mutex_unlock(osal_mutex_t *m) { assert(!pthread_mutex_unlock(&m->mutex)); }
osal_sem_t *osal_sem_create(uint32_t initial, uint32_t max) {
    assert(initial == 0 && max == 1);
    assert(!pthread_mutex_init(&done.mutex, NULL));
    assert(!pthread_cond_init(&done.cond, NULL));
    return &done;
}
osal_err_t osal_sem_take(osal_sem_t *s, int64_t timeout) {
    assert(timeout == OSAL_WAIT_FOREVER);
    pthread_mutex_lock(&gate);
    stopping = true;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&gate);
    pthread_mutex_lock(&s->mutex);
    while (!s->signaled) pthread_cond_wait(&s->cond, &s->mutex);
    s->signaled = false;
    pthread_mutex_unlock(&s->mutex);
    return OSAL_OK;
}
void osal_sem_give(osal_sem_t *s) {
    pthread_mutex_lock(&s->mutex);
    s->signaled = true;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);
}
static void *run_task(void *arg) { (void)arg; worker_cfg.fn(worker_cfg.arg); return NULL; }
osal_err_t osal_task_create(const osal_task_cfg_t *cfg, osal_task_t **out) {
    assert(!out && cfg->stack_bytes == 4096 && cfg->flags == OSAL_TASK_STACK_EXTERNAL);
    if (task_fail) return OSAL_NO_MEM;
    worker_cfg = *cfg;
    pthread_mutex_lock(&gate);
    stopping = false;
    pthread_mutex_unlock(&gate);
    assert(!pthread_create(&worker, NULL, run_task, NULL));
    threaded = true;
    return OSAL_OK;
}
void osal_panic(const char *fmt, ...) { (void)fmt; abort(); }
bool metalio_board_ready(void) { return true; }
bool metalio_board_rails_cut(void) { return cut; }
uint8_t metalio_board_output(int port) { assert(port == 1); return powered ? MB_P1_SHARED_PWR : 0; }
unsigned metalio_board_shared_power_epoch(void) { return epoch; }
uint64_t metalio_board_shared_power_since_us(void) { return powered_since; }
hal_i2c_status_t metalio_board_set(int port, uint8_t mask, uint8_t value) {
    assert(port == 0 && mask == MB_P0_AMP);
    amp = value != 0;
    event(amp ? 'A' : 'a');
    return HAL_I2C_OK;
}
void hal_power_request(hal_power_res_t r, bool on) { holds[r] += on ? 1 : -1; assert(holds[r] >= 0); }
hal_uart_t *hal_uart_open(int port, int baud) {
    assert(port == 1 && baud == 115200);
    return (hal_uart_t *)&mutex;
}
int hal_uart_write(hal_uart_t *u, const void *data, size_t n) {
    assert(u);
    if (n == 9 && !memcmp(data, "AT+RX=2\r\n", n)) { mode_calls++; event('b'); }
    else { assert(n == 11 && !memcmp(data, "AT+MODE=1\r\n", n)); event('m'); }
    return uart_fail ? -1 : (int)n;
}
int hal_uart_read_line(hal_uart_t *u, char *out, size_t cap, uint32_t ms) {
    (void)u;
    __atomic_add_fetch(&now, (uint64_t)ms * 1000, __ATOMIC_RELAXED);
    if (!ack) return -1;
    snprintf(out, cap, "SET MODE 1");
    return 10;
}
esp_err_t i2s_new_channel(const i2s_chan_config_t *c, i2s_chan_handle_t *t, i2s_chan_handle_t *r) {
    assert(c->id == 0 && c->role == I2S_ROLE_SLAVE && c->dma_desc_num == 4 && c->dma_frame_num == 240 && c->auto_clear_after_cb);
    if (fail()) return ESP_FAIL;
    tx = (struct fake_i2s){.tx = true};
    rx = (struct fake_i2s){0};
    *t = &tx;
    *r = &rx;
    event('N');
    return ESP_OK;
}
esp_err_t i2s_del_channel(i2s_chan_handle_t c) { assert(!c->enabled); event(c->tx ? 'T' : 'R'); return ESP_OK; }
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t c, const i2s_std_config_t *s) {
    (void)c;
    assert(s->clk_cfg.sample_rate_hz == 16000 && s->slot_cfg.data_bit_width == 32 && s->slot_cfg.slot_mode == 2);
    assert(s->gpio_cfg.mclk == -1 && s->gpio_cfg.bclk == 42 && s->gpio_cfg.ws == 43 && s->gpio_cfg.dout == 44 && s->gpio_cfg.din == 45);
    return fail() ? ESP_FAIL : ESP_OK;
}
esp_err_t i2s_channel_register_event_callback(i2s_chan_handle_t c, const i2s_event_callbacks_t *cb, void *ctx) {
    assert(c == &rx && !ctx);
    lost_cb = cb->on_recv_q_ovf;
    return fail() ? ESP_FAIL : ESP_OK;
}
esp_err_t i2s_channel_preload_data(i2s_chan_handle_t c, const void *data, size_t n, size_t *bytes) {
    assert(c->tx && !c->enabled && n == 240 * 8);
    const unsigned char *p = data;
    for (size_t i = 0; i < n; i++) assert(!p[i]);
    if (fail()) return ESP_FAIL;
    c->preloads++;
    *bytes = n;
    return ESP_OK;
}
esp_err_t i2s_channel_enable(i2s_chan_handle_t c) {
    assert(!c->enabled);
    if (c->tx) assert(c->preloads == 4);
    if (fail()) return ESP_FAIL;
    c->enabled = true;
    event(c->tx ? 'E' : 'e');
    return ESP_OK;
}
esp_err_t i2s_channel_disable(i2s_chan_handle_t c) {
    assert(c->enabled);
    c->enabled = false;
    c->preloads = 0;
    event(c->tx ? 'D' : 'd');
    return ESP_OK;
}
esp_err_t i2s_channel_write(i2s_chan_handle_t c, const void *data, size_t n, size_t *bytes, uint32_t timeout) {
    assert(c == &tx && c->enabled && amp && n % 8 == 0);
    (void)timeout;
    const int32_t *p = data;
    for (size_t i = 0; i < n / 4; i += 2) assert(p[i] == p[i + 1] && p[i] == -32768 * 65536LL);
    *bytes = n < write_limit ? n : write_limit;
    event('W');
    return *bytes == n ? ESP_OK : ESP_ERR_TIMEOUT;
}
esp_err_t i2s_channel_read(i2s_chan_handle_t c, void *data, size_t n, size_t *bytes, uint32_t timeout) {
    assert(c == &rx && c->enabled && n == 240 * 8 && timeout == 250);
    pthread_mutex_lock(&gate);
    while (!feed && !stopping) pthread_cond_wait(&changed, &gate);
    if (stopping) { *bytes = 0; pthread_mutex_unlock(&gate); return ESP_ERR_TIMEOUT; }
    int32_t *p = data;
    for (size_t i = 0; i < n / 4; i += 2) { p[i] = INT32_MIN; p[i + 1] = 123 * 4096; }
    *bytes = n;
    feed = false;
    if (overflow) { i2s_event_data_t ev = {.size = 240 * 8}; assert(!lost_cb(c, &ev, NULL)); }
    pthread_mutex_unlock(&gate);
    return ESP_OK;
}
static void captured(void *ctx, const int16_t *mic, const int16_t *ref, int n, int lost, uint64_t t) {
    assert(ctx == &mutex && n == 240 && ref && t <= osal_now_us());
    for (int i = 0; i < n; i++) assert(mic[i] == -32767 && ref[i] == 123);
    pthread_mutex_lock(&gate);
    callbacks++;
    lost_samples += lost;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&gate);
}
static void stop_capture(void) {
    hal_audio_capture_stop();
    if (threaded) { assert(!pthread_join(worker, NULL)); threaded = false; }
}
static void idle(void) {
    assert(!amp && !allocations && !tx.enabled && !rx.enabled);
    assert(!holds[HAL_PWR_AUDIO_IN] && !holds[HAL_PWR_AUDIO_OUT]);
}
int main(void) {
    ink_s31_audio_init();
    hal_audio_info_t info;
    hal_audio_get_info(&info);
    assert(info.has_speaker && info.has_mic && info.mic_reference && info.chunk_samples == 240 && info.runway_ms == 60);
    assert(hal_audio_route_set(HAL_ROUTE_SPEAKER) == 0 && hal_audio_route_set(HAL_ROUTE_BT_HEADSET) == -1);
    int16_t pcm[480];
    for (size_t i = 0; i < 480; i++) pcm[i] = INT16_MIN;
    assert(hal_audio_play_write(pcm, 1, 0) == -1);
    assert(hal_audio_capture_start(NULL, NULL) == -1);
    powered = false;
    assert(hal_audio_play_start() == -1);
    powered = true;
    cut = true;
    assert(hal_audio_play_start() == -1);
    cut = false;
    ack = false;
    assert(hal_audio_play_start() == -1);
    idle();
    ack = true;
    trace_n = 0;
    assert(hal_audio_play_start() == 0 && !amp && holds[HAL_PWR_AUDIO_OUT] == 1);
    assert(strstr(trace, "bwmN") && hal_audio_play_start() == -1);
    write_limit = 80;
    assert(hal_audio_play_write(pcm, 480, 0) == 10);
    assert(hal_audio_play_delay_us() == 60000);
    write_limit = SIZE_MAX;
    assert(hal_audio_play_write(pcm, 480, 0) == 480);
    delayed = 0;
    hal_audio_play_stop(true);
    assert(delayed == 60000 && strstr(trace, "EAW") && strstr(trace, "wDaTR"));
    idle();
    for (int i = 1; i <= 9; i++) {
        fail_step = i;
        step = 0;
        assert(hal_audio_play_start() == -1);
        idle();
    }
    fail_step = 0;
    for (int i = 1; i <= 3; i++) { fail_alloc = i; assert(hal_audio_play_start() == -1); idle(); }
    task_fail = true;
    assert(hal_audio_capture_start(captured, &mutex) == -1);
    idle();
    task_fail = false;
    assert(hal_audio_capture_start(captured, &mutex) == 0 && !amp);
    assert(hal_audio_capture_start(captured, &mutex) == -1);
    assert(hal_audio_play_start() == 0);
    assert(hal_audio_play_write(pcm, 240, 0) == 240);
    trace_n = 0;
    hal_audio_play_stop(false);
    assert(!amp && rx.enabled && !tx.enabled && !strchr(trace, 'w') && !strchr(trace, 'T'));
    assert(hal_audio_play_start() == 0); /* stale DMA is cleared while capture keeps RX alive */
    pthread_mutex_lock(&gate);
    feed = overflow = true;
    pthread_cond_broadcast(&changed);
    while (!callbacks) pthread_cond_wait(&changed, &gate);
    pthread_mutex_unlock(&gate);
    stop_capture();
    assert(callbacks == 1 && lost_samples == 240 && tx.enabled && !rx.enabled);
    hal_audio_play_stop(false);
    idle();
    int old_modes = mode_calls;
    epoch++;
    powered_since = osal_now_us();
    uart_fail = true;
    assert(hal_audio_play_start() == -1);
    uart_fail = false;
    assert(hal_audio_play_start() == 0 && mode_calls == old_modes + 2);
    hal_audio_play_stop(false);
    stop_capture();
    idle();
    puts("audio: lifecycle, full duplex, DMA, PA order, mode ACK/epoch and failures passed");
    return 0;
}
