/* GPIO handlers only notify; this task samples I2C, debounces and delivers the HAL edges. */
#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ink/hal_device.h"
#include "ink/osal.h"
#include "s31_board.h"
#include "s31_input_logic.h"
#include "s31_io.h"
#include "s31_bh1098.h"
#include "vendor/board_874538e/input_binding.inc"

enum { INPUT_EXP = 1, INPUT_TOUCH = 2, INPUT_AI = 4, INPUT_RESUME = 8, INPUT_MOTOR = 16 };
static TaskHandle_t g_task;
static SemaphoreHandle_t g_sample;
static StaticSemaphore_t g_sample_store;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static hal_input_sink_fn g_sink;
static void *g_ctx;
static hal_i2c_dev_t *g_touch;
static s31_keys_t g_keys;
static s31_contact_t g_contacts[FT6336U_MAX_POINTS];
static bool g_touch_poll, g_keys_back = true, g_resuming;
static uint32_t g_wake;
static uint64_t g_wake_at;
static int g_motor_ms;
static bool g_motor_requested;

/* Every key edge and every touch down and up, for the device log (moves are too many). */
static void log_event(const hal_input_event_t *e) {
    static const char *const names[HAL_KEY_COUNT] = {"none", "ai", "power", "vol+", "vol-", "home", "prev", "next", "back"};
    char line[64];
    if (e->type == HAL_IN_KEY)
        snprintf(line, sizeof line, "input: key %s %s", (unsigned)e->key.key < HAL_KEY_COUNT ? names[e->key.key] : "?",
                 e->key.pressed ? "down" : "up");
    else if (e->type == HAL_IN_TOUCH && e->touch.phase != HAL_TOUCH_MOVE)
        snprintf(line, sizeof line, "input: touch %s %d,%d", e->touch.phase == HAL_TOUCH_DOWN ? "down" : "up", e->touch.x, e->touch.y);
    else
        return;
    hal_log_write(line);
}

static void emit(const hal_input_event_t *e, void *ctx) {
    (void)ctx;
    log_event(e);
    portENTER_CRITICAL(&g_mux);
    hal_input_sink_fn sink = g_sink;
    void *sink_ctx = g_ctx;
    portEXIT_CRITICAL(&g_mux);
    if (sink) sink(e, sink_ctx);
}

static void irq(void *ctx) { xTaskNotify(g_task, (uint32_t)(uintptr_t)ctx, eSetBits); }
void ink_s31_bh_wake(void) { if (g_task) xTaskNotify(g_task, 0, eSetBits); }

static void motor_poll(uint64_t now) { /* sampling lock held; I2C never runs in the caller */
    (void)now;
    portENTER_CRITICAL(&g_mux);
    int ms = g_motor_ms;
    g_motor_ms = 0;
    portEXIT_CRITICAL(&g_mux);
    if (ms > 0) {
        ink_vendor_vibe_SetIntensity(ms >= 70 ? 3 : ms >= 50 ? 2 : 1);
        ink_vendor_vibe_Pulse(ms);
    }
    __atomic_store_n(&g_motor_requested, pulse_active_ || continuous_, __ATOMIC_RELEASE);
}

static void sample_keys(uint64_t now) {
    if (g_resuming && g_wake) {
        uint32_t wake_keys = (g_wake & HAL_WAKE_POWER_KEY ? 1u << HAL_KEY_POWER : 0) |
                             (g_wake & HAL_WAKE_AI_KEY ? 1u << HAL_KEY_AI : 0);
        s31_keys_report(&g_keys, g_keys.stable | wake_keys, g_wake_at, emit, NULL);
        g_wake = 0;
    }
    uint8_t p1;
    if (!g_keys_back || metalio_board_read(1, &p1) != HAL_I2C_OK) {
        g_keys.pending = g_keys_back;
        return;
    }
    uint32_t mask = s31_key_mask(p1, !gpio_get_level(S31_GPIO_AI_KEY));
    if (g_resuming) {
        s31_keys_report(&g_keys, mask, now, emit, NULL);
        g_keys.candidate = mask;
        g_keys.pending = g_resuming = false;
    } else {
        sample_all();
        s31_keys_report(&g_keys, ink_vendor_key_mask(), now, emit, NULL);
    }
}

/* The touch controller's frame. A new contact counts from the INT edge when that is newer than the last read. */
static void read_touch(void) {
    static uint64_t last_read;
    uint64_t now = osal_now_us(), edge = s31_irq_edge_us(S31_GPIO_TOUCH_INT);
    ft6336u_point_t pts[FT6336U_MAX_POINTS];
    int n = 0;
    if (ft6336u_read(g_touch, pts, FT6336U_MAX_POINTS, &n) == HAL_I2C_OK) {
        for (int i = 0; i < n; ++i)
            if (!metalio_touch_cover_key(pts[i].x, pts[i].y))
                map_to_logical(pts[i].x, pts[i].y, &pts[i].x, &pts[i].y);
        s31_touch_frame_at(g_contacts, pts, n, osal_now_us(), edge > last_read && edge <= now ? edge : now, emit, NULL);
        g_touch_poll = n > 0;
    } else g_touch_poll = true;
    last_read = now;
}

static void input_task(void *arg) {
    (void)arg;
    TickType_t wait = portMAX_DELAY;
    for (;;) {
        uint32_t bits = 0;
        xTaskNotifyWait(0, UINT32_MAX, &bits, wait);
        ink_s31_bh_poll(); /* try-lock, buffered UART only; never wait on a mode command */
        xSemaphoreTake(g_sample, portMAX_DELAY);
        uint64_t now = osal_now_us();
        if ((bits & INPUT_EXP) && g_keys_back) {
            s31_accel_take_motion();
            uint8_t p0;
            metalio_board_read(0, &p0); /* release PWR_GOOD's interrupt too */
        }
        bool touch_first = g_touch && (bits & INPUT_TOUCH); /* woken by the touch INT: its frame before the keys' reads */
        if (touch_first) read_touch();
        if (metalio_board_ready()) sample_keys(now);
        if (g_touch && !touch_first) read_touch();
        if (metalio_board_ready()) motor_poll(osal_now_us());
        wait = g_touch_poll || s31_haptic_busy() || ink_s31_bh_headset() ? pdMS_TO_TICKS(S31_TOUCH_POLL_MS) :
               g_touch ? pdMS_TO_TICKS(20) : pdMS_TO_TICKS(50);
        xSemaphoreGive(g_sample);
    }
}

void s31_input_init(hal_i2c_dev_t *touch) {
    g_touch = touch;
    ink_vendor_boot_gpio();
    ink_vendor_keys_begin(true);
    ink_vendor_vibe_create();
    ink_vendor_vibe_Init();
    g_sample = xSemaphoreCreateMutexStatic(&g_sample_store);
    if (!g_sample || xTaskCreatePinnedToCore(input_task, "input-hal", 4096, NULL, OSAL_PRIO_MAX, &g_task, 0) != pdPASS)
        osal_panic("input HAL: task setup failed");
    if (s31_irq_watch(S31_GPIO_AI_KEY, irq, (void *)(uintptr_t)INPUT_AI) ||
        s31_irq_watch(S31_GPIO_EXP_INT, irq, (void *)(uintptr_t)INPUT_EXP) ||
        (touch && s31_irq_watch_task(S31_GPIO_TOUCH_INT, g_task, INPUT_TOUCH)))
        osal_panic("input HAL: interrupt setup failed");
    xTaskNotify(g_task, INPUT_EXP | INPUT_TOUCH, eSetBits);
}

void hal_input_set_sink(hal_input_sink_fn fn, void *ctx) {
    portENTER_CRITICAL(&g_mux);
    g_sink = fn;
    g_ctx = ctx;
    portEXIT_CRITICAL(&g_mux);
}

void hal_input_get_info(hal_input_info_t *out) {
    *out = (hal_input_info_t){.has_touch = g_touch != NULL, .touch_points = g_touch ? FT6336U_MAX_POINTS : 0,
        .keys = (1u << HAL_KEY_AI) | (1u << HAL_KEY_POWER) | (1u << HAL_KEY_VOL_UP) | (1u << HAL_KEY_VOL_DOWN)};
    if (g_touch) out->keys |= (1u << HAL_KEY_HOME) | (1u << HAL_KEY_PREV) | (1u << HAL_KEY_NEXT);
}

void s31_input_pause(void) { xSemaphoreTake(g_sample, portMAX_DELAY); ink_vendor_vibe_Set(false); }
void s31_input_resume(uint32_t wake, uint64_t t, bool keys_back) {
    g_keys_back = keys_back;
    if (keys_back) g_keys_back = ink_vendor_keys_begin(false);
    g_resuming = true;
    g_wake = wake;
    g_wake_at = t;
    xTaskNotify(g_task, INPUT_RESUME, eSetBits);
    xSemaphoreGive(g_sample);
}

bool hal_haptic_available(void) { return g_task && metalio_board_ready(); }
bool s31_haptic_busy(void) { return __atomic_load_n(&g_motor_requested, __ATOMIC_ACQUIRE); }
void hal_haptic_pulse(int ms) {
    if (!hal_haptic_available()) return;
    portENTER_CRITICAL(&g_mux);
    int duration = ms > 0 ? ms : MB_MOTOR_PULSE_MS;
    if (duration > g_motor_ms) g_motor_ms = duration;
    __atomic_store_n(&g_motor_requested, true, __ATOMIC_RELEASE);
    portEXIT_CRITICAL(&g_mux);
    xTaskNotify(g_task, INPUT_MOTOR, eSetBits);
}
