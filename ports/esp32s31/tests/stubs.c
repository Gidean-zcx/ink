/* Remaining provisioning placeholders must not fabricate success. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "ink/hal_cellular.h"
#include "ink/hal_net.h"
#include "ink/hal_provision.h"
static uint64_t waited;
void osal_task_delay_us(uint64_t us) { waited += us; }
void esp_log_write(int level, const char *tag, const char *format, ...) { (void)level; (void)tag; (void)format; abort(); }
void esp_fill_random(void *out, size_t size) { (void)out; (void)size; abort(); }
int main(void) {
    assert(hal_provision_start("test-only-setup", "test-only-password", NULL, NULL) == -1);
    assert(waited == 0);
    puts("s31 provisioning stubs: unavailable without fabricated success; WebSocket tested separately");
    return 0;
}
