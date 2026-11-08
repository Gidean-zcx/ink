#include "ink/sys/hub.h"
#include <stdlib.h>
#include <string.h>
static int controlled_subscribe(const char *prefix, ink_hub_notify_fn fn, void *ctx) {
    const char *phase = getenv("INK_CONFIRM_PHASE");
    if (phase && strstr(phase, "capacity") && !strcmp(prefix, "device/event_ack")) return 0;
    return ink_hub_subscribe(prefix, fn, ctx);
}
#define ink_hub_subscribe controlled_subscribe
#ifndef INK_TEST_CAP_SOURCE
#define INK_TEST_CAP_SOURCE "../../system/policy/cap.c"
#endif
#include INK_TEST_CAP_SOURCE
#undef ink_hub_subscribe
void review_tools(const ink_json_t *j) { on_tools(NULL, 101, "tools/call", j, 0); }
bool review_sent(uint32_t token) {
    bool sent = false;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < EVENTS_KEPT; i++) {
        int64_t t = 0;
        if (EV[i].delivered && EV[i].seq && EV[i].seq <= ev_sent && !strcmp(EV[i].id, "confirm.result") &&
            ink_args_get_int(&EV[i].args, "token", &t) && t == token) sent = true;
    }
    osal_mutex_unlock(CB.lock);
    return sent;
}
void review_forget_delivery(uint32_t token) { /* Model an event skipped by the member filter. */
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < EVENTS_KEPT; i++) {
        int64_t t = 0;
        if (ink_args_get_int(&EV[i].args, "token", &t) && t == token) EV[i].delivered = false;
    }
    osal_mutex_unlock(CB.lock);
}
void test_event_ack(bool valid) {
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    char text[100];
    snprintf(text, sizeof text, "{\"boot\":%u,\"upto\":%u}", CB.boot_tag + !valid, ev_sent);
    if (j && !ink_json_parse(j, text, strlen(text))) event_ack(NULL, "device/event_ack", j, 0);
    osal_free(j);
}
