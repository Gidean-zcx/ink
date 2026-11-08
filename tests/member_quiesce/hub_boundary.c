/* Hold an actual request at the final socket send, preserving its identity. */
#include <string.h>
#include "ink/osal.h"
#include "ink/hal_net.h"
static bool armed, waiting, released;
static int send_held(hal_ws_t *ws, bool binary, const void *data, size_t len, int64_t timeout_us) {
    char head[128];
    size_t n = len < sizeof head - 1 ? len : sizeof head - 1;
    memcpy(head, data, n); head[n] = 0;
    if (!binary && armed && strstr(head, "\"method\":\"ai.ask\"")) {
        armed = false; waiting = true;
        while (!released) osal_task_delay_us(1000);
    }
    return hal_ws_send(ws, binary, data, len, timeout_us);
}
#define hal_ws_send send_held
#ifndef INK_TEST_HUB_SOURCE
#define INK_TEST_HUB_SOURCE "../../system/services/hub/hub_link.c"
#endif
#include INK_TEST_HUB_SOURCE
#undef hal_ws_send
void test_upload_pause(void) { armed = true; waiting = released = false; }
void test_upload_release(void) { released = true; }
bool test_upload_waiting(void) { return waiting; }
