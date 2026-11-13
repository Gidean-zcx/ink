/*
 * GPIO interrupt lines to one task. The ISR only sets the line's bit and wakes the task; the
 * handlers (I2C reads) run there. A line still low after its service (a read failed, so the chip
 * still holds it) gets serviced again, 20 ms later and backing off to once a second, as the
 * simulator's GPIO2 recheck does (hal_input_sim.c).
 * A line given to a task (s31_irq_watch_task) skips this task: the ISR notifies that task itself and notes the
 * edge's time (tap-loss-1: during a refresh the panel tasks at priority 23-24 busy-wait on core 0, so this task, at
 * 10, never ran and a touch waited for the input task's 20 ms poll). That task looks at a line left low itself.
 */
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "s31_board.h"

#define MAX_LINES 4
#define STACK_BYTES 4096
#define PRIO 10 /* OSAL_PRIO_HIGH: key, touch and wake lines */
#define RECHECK_MS 20
#define RECHECK_MAX_MS 1000

static struct {
    int gpio;
    s31_irq_fn fn;
    void *ctx;
    TaskHandle_t task; /* notified straight from the ISR with `bits` */
    uint32_t bits;
    volatile int64_t edge_us;
} g_lines[MAX_LINES];
static int g_n;
static TaskHandle_t g_task;

/* esp_timer_get_time is in IRAM with CONFIG_ESP_TIMER_IN_IRAM=y (IDF esp_timer_impl_systimer.c:68-75). The GPIO ISR
 * service is installed without ESP_INTR_FLAG_IRAM: during a flash operation the edge waits until it ends. */
static void IRAM_ATTR isr(void *arg) {
    BaseType_t woken = pdFALSE;
    uintptr_t i = (uintptr_t)arg;
    if (g_lines[i].task) {
        g_lines[i].edge_us = esp_timer_get_time();
        xTaskNotifyFromISR(g_lines[i].task, g_lines[i].bits, eSetBits, &woken);
    } else {
        xTaskNotifyFromISR(g_task, 1u << i, eSetBits, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

uint64_t s31_irq_edge_us(int gpio) {
    for (int i = 0; i < g_n; i++)
        if (g_lines[i].gpio == gpio) return (uint64_t)g_lines[i].edge_us;
    return 0;
}

static void irq_task(void *arg) {
    (void)arg;
    uint32_t low = 0, gap_ms = RECHECK_MS;
    for (;;) {
        uint32_t bits = 0;
        TickType_t wait = low ? pdMS_TO_TICKS(gap_ms) : portMAX_DELAY;
        if (xTaskNotifyWait(0, UINT32_MAX, &bits, wait) == pdTRUE) gap_ms = RECHECK_MS;
        else gap_ms = gap_ms * 2 > RECHECK_MAX_MS ? RECHECK_MAX_MS : gap_ms * 2;
        bits |= low;
        low = 0;
        for (int i = 0; i < g_n; i++) {
            if (!(bits & (1u << i)) || g_lines[i].task) continue;
            g_lines[i].fn(g_lines[i].ctx);
            if (!gpio_get_level(g_lines[i].gpio)) low |= 1u << i;
        }
    }
}

static int watch(int gpio, s31_irq_fn fn, void *ctx, TaskHandle_t task, uint32_t bits) {
    if ((!fn && !task) || g_n == MAX_LINES) return -1;
    if (fn && !g_task && xTaskCreatePinnedToCore(irq_task, "irq", STACK_BYTES, NULL, PRIO, &g_task, 0) != pdPASS) return -1;
    static bool service;
    if (!service) {
        esp_err_t e = gpio_install_isr_service(0);
        if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return -1; /* INVALID_STATE: already installed */
        service = true;
    }
    int i = g_n;
    g_lines[i].gpio = gpio;
    g_lines[i].fn = fn;
    g_lines[i].ctx = ctx;
    g_lines[i].task = task;
    g_lines[i].bits = bits;
    gpio_config_t c = {.pin_bit_mask = 1ull << gpio, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE,
                       .intr_type = gpio == S31_GPIO_AI_KEY ? GPIO_INTR_ANYEDGE : GPIO_INTR_NEGEDGE};
    g_n = i + 1; /* before the handler: its first edge must find the line */
    if (gpio_config(&c) != ESP_OK || gpio_isr_handler_add(gpio, isr, (void *)(uintptr_t)i) != ESP_OK) {
        g_n = i;
        return -1;
    }
    if (!gpio_get_level(gpio)) xTaskNotify(task ? task : g_task, task ? bits : 1u << i, eSetBits); /* already low: no edge will come */
    return 0;
}

int s31_irq_watch(int gpio, s31_irq_fn fn, void *ctx) { return watch(gpio, fn, ctx, NULL, 0); }
int s31_irq_watch_task(int gpio, void *task, uint32_t bits) { return task ? watch(gpio, NULL, NULL, task, bits) : -1; }
