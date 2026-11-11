/* Remaining WebSocket placeholders must not fabricate a connection; cell.c tests the modem. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "ink/hal_cellular.h"
#include "ink/hal_net.h"
static uint64_t waited;
void osal_task_delay_us(uint64_t us) { waited += us; }
void esp_log_write(int level, const char *tag, const char *format, ...) { (void)level; (void)tag; (void)format; abort(); }
void esp_fill_random(void *out, size_t size) { (void)out; (void)size; abort(); }
int main(void) {
    assert(!hal_ws_open("wss://example.com", NULL, NULL));
    assert(!hal_ws_open_pinned("wss://example.com", NULL, NULL, NULL));
    assert(waited == 0);
    puts("s31 WebSocket stubs: unavailable without fabricated connections");
    return 0;
}
