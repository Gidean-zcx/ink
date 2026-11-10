/* GPIO handlers only notify; this task samples I2C, debounces and delivers the HAL edges. */
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ink/hal_device.h"
#include "ink/osal.h"
#include "s31_board.h"
#include "s31_input_logic.h"
#include "s31_io.h"

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
static uint64_t g_wake_at, g_motor_until;
static bool g_motor_on, g_motor_requested;

static void emit(const hal_input_event_t *e, void *ctx) {
    (void)ctx;
    portENTER_CRITICAL(&g_mux);
    hal_input_sink_fn sink = g_sink;
    void *sink_ctx = g_ctx;
    portEXIT_CRITICAL(&g_mux);
    if (sink) sink(e, sink_ctx);
}

static void irq(void *ctx) { xTaskNotify(g_task, (uint32_t)(uintptr_t)ctx, eSetBits); }

static void motor_poll(uint64_t now) { /* sampling lock held; I2C never runs in the caller */
    portENTER_CRITICAL(&g_mux);
    bool on = __atomic_load_n(&g_motor_requested, __ATOMIC_ACQUIRE) && now < g_motor_until;
    portEXIT_CRITICAL(&g_mux);
    if (on != g_motor_on && metalio_board_set(0, MB_P0_MOTOR, on ? MB_P0_MOTOR : 0) == HAL_I2C_OK)
        g_motor_on = on;
    portENTER_CRITICAL(&g_mux);
    if (!g_motor_on && now >= g_motor_until) __atomic_store_n(&g_motor_requested, false, __ATOMIC_RELEASE);
    portEXIT_CRITICAL(&g_mux);
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
    } else s31_keys_sample(&g_keys, mask, now, emit, NULL);
}

static void input_task(void *arg) {
    (void)arg;
    TickType_t wait = portMAX_DELAY;
    for (;;) {
        uint32_t bits = 0;
        xTaskNotifyWait(0, UINT32_MAX, &bits, wait);
        xSemaphoreTake(g_sample, portMAX_DELAY);
        uint64_t now = osal_now_us();
        if ((bits & INPUT_EXP) && g_keys_back) {
            s31_accel_take_motion();
            uint8_t p0;
            metalio_board_read(0, &p0); /* release PWR_GOOD's interrupt too */
        }
        if (metalio_board_ready() && ((bits & (INPUT_EXP | INPUT_AI | INPUT_RESUME)) || g_keys.pending ||
                                     (g_keys.stable & (1u << HAL_KEY_AI)))) sample_keys(now);
        if (g_touch && (g_touch_poll || (bits & (INPUT_TOUCH | INPUT_RESUME)))) {
            ft6336u_point_t pts[FT6336U_MAX_POINTS];
            int n = 0;
            if (ft6336u_read(g_touch, pts, FT6336U_MAX_POINTS, &n) == HAL_I2C_OK) {
                s31_touch_frame(g_contacts, pts, n, osal_now_us(), emit, NULL);
                g_touch_poll = n > 0;
            } else g_touch_poll = true;
        }
        if (metalio_board_ready()) motor_poll(osal_now_us());
        wait = g_touch_poll || s31_haptic_busy() ? pdMS_TO_TICKS(S31_TOUCH_POLL_MS) :
               g_keys.pending || (g_keys.stable & (1u << HAL_KEY_AI)) ? pdMS_TO_TICKS(S31_KEY_DEBOUNCE_US / 1000) : portMAX_DELAY;
        xSemaphoreGive(g_sample);
    }
}

void s31_input_init(hal_i2c_dev_t *touch) {
    g_touch = touch;
    g_sample = xSemaphoreCreateMutexStatic(&g_sample_store);
    if (!g_sample || xTaskCreatePinnedToCore(input_task, "input-hal", 4096, NULL, OSAL_PRIO_MAX, &g_task, 0) != pdPASS)
        osal_panic("input HAL: task setup failed");
    if (s31_irq_watch(S31_GPIO_AI_KEY, irq, (void *)(uintptr_t)INPUT_AI) ||
        s31_irq_watch(S31_GPIO_EXP_INT, irq, (void *)(uintptr_t)INPUT_EXP) ||
        (touch && s31_irq_watch(S31_GPIO_TOUCH_INT, irq, (void *)(uintptr_t)INPUT_TOUCH)))
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

void s31_input_pause(void) { xSemaphoreTake(g_sample, portMAX_DELAY); }
void s31_input_resume(uint32_t wake, uint64_t t, bool keys_back) {
    g_keys_back = keys_back;
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
    uint64_t until = osal_now_us() + (uint64_t)(ms > 0 ? ms : MB_MOTOR_PULSE_MS) * 1000u;
    portENTER_CRITICAL(&g_mux);
    if (until > g_motor_until) g_motor_until = until;
    __atomic_store_n(&g_motor_requested, true, __ATOMIC_RELEASE);
    portEXIT_CRITICAL(&g_mux);
    xTaskNotify(g_task, INPUT_MOTOR, eSetBits);
}
