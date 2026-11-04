/* Check the HAL contract independently of hub's proactive HE_LINK close. */
#include <stdio.h>
#include "ink/hal_net.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "sim.h"

static osal_sem_t *connected, *closed;
static int connects, closes;

static void event(void *ctx, hal_ws_event_t ev, const uint8_t *data, size_t len) {
    (void)ctx; (void)data; (void)len;
    if (ev == HAL_WS_CONNECTED) { connects++; osal_sem_give(connected); }
    if (ev == HAL_WS_CLOSED) { closes++; osal_sem_give(closed); }
}

static void audit_task(void *ctx) {
    (void)ctx;
    hal_net_use(HAL_LINK_WIFI);
    osal_task_delay_us(3000000);
    hal_ws_t *ws = hal_ws_open("wss://hub.local/v1/device", event, NULL);
    if (!ws || osal_sem_take(connected, 5000000) != OSAL_OK) osal_panic("audit websocket open");
    hal_link_status_t st;
    hal_net_link_status(&st);
    printf("AUDIT_HAL before_switch type=%d up=%d connected=%d closed=%d\n", st.type, st.up, connects, closes);
    hal_net_use(HAL_LINK_LTE);
    int waited = osal_sem_take(closed, 6000000);
    hal_net_link_status(&st);
    printf("AUDIT_HAL after_switch type=%d up=%d connected=%d closed=%d wait=%d\n", st.type, st.up, connects, closes, waited);
    if (st.type == HAL_LINK_LTE && st.up && connects == 1 && closes == 1 && waited == OSAL_OK)
        ink_probe_count("audit.pass");
    else {
        ink_probe_count("audit.fail");
        sim_warn("old WebSocket survived a change between two available networks");
    }
    hal_ws_free(ws);
    ink_probe_count("audit.finished");
}

void ink_main(void) {
    connected = osal_sem_create(0,1);
    closed = osal_sem_create(0,1);
    osal_task_cfg_t c={.name="audit-hal",.fn=audit_task,.stack_bytes=4096,.priority=3,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    if(osal_task_create(&c,NULL)!=OSAL_OK) osal_panic("audit task");
}
