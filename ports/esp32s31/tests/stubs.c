/* A physical modem without its transport must neither spin nor fabricate events. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "ink/hal_cellular.h"
static uint64_t waited;
void osal_task_delay_us(uint64_t us) { waited += us; }
void esp_log_write(int level, const char *tag, const char *format, ...) { (void)level; (void)tag; (void)format; abort(); }
void esp_fill_random(void *out, size_t size) { (void)out; (void)size; abort(); }
int main(void) {
    hal_cell_event_t event = {.type = HAL_CELL_EV_RING};
    assert(hal_cell_wait_event(&event, 1000) == 0);
    assert(waited == 1000000 && event.type == HAL_CELL_EV_RING);
    puts("s31 unavailable modem: waits without fabricating an event");
    return 0;
}
