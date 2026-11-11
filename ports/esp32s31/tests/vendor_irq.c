/* The actual GPIO subscription must preserve the vendor BOOT both-edge mode. */
#include <assert.h>
#include <string.h>
#include "vendor_idf.h"
#include "../s31_irq.c"
static int watched, edge;
static uint32_t notified;
BaseType_t xTaskCreatePinnedToCore(void (*fn)(void *), const char *name, unsigned stack, void *arg, int priority, TaskHandle_t *out, int core) {
    (void)arg; assert(fn && !strcmp(name, "irq") && stack == 4096 && priority == 10 && core == 0); *out = (void *)1; return pdPASS;
}
BaseType_t xTaskNotifyWait(uint32_t a, uint32_t b, uint32_t *bits, TickType_t ticks) { (void)a; (void)b; (void)bits; (void)ticks; return pdFALSE; }
BaseType_t xTaskNotify(TaskHandle_t t, uint32_t bits, int mode) { assert(t && mode == eSetBits); notified |= bits; return pdTRUE; }
void xTaskNotifyFromISR(TaskHandle_t t, uint32_t bits, int mode, BaseType_t *woken) { assert(t && mode == eSetBits); notified |= bits; *woken = pdTRUE; }
esp_err_t gpio_install_isr_service(int flags) { assert(flags == 0); return ESP_OK; }
esp_err_t gpio_isr_handler_add(int pin, void (*handler)(void *), void *arg) { assert(pin == watched && handler); handler(arg); return ESP_OK; }
esp_err_t gpio_config(const gpio_config_t *c) {
    watched = c->pin_bit_mask == (1ULL << 61) ? 61 : 5;
    edge = c->intr_type; return ESP_OK;
}
int gpio_get_level(int pin) { assert(pin == watched); return 0; }
static void sample(void *arg) { (void)arg; }
int main(void) {
    assert(s31_irq_watch(61, sample, NULL) == 0 && edge == GPIO_INTR_ANYEDGE && notified == 1);
    assert(s31_irq_watch(5, sample, NULL) == 0 && edge == GPIO_INTR_NEGEDGE && notified == 3);
    return 0;
}
