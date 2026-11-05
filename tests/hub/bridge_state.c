/* Exercise late network callbacks without opening any host socket. */
#include "sim.h"
#include "../../ports/sim/hal/hal_net_sim.c"

static struct hal_ws test_ws;

static void check_callbacks(void *arg) {
    (void)arg;
    test_ws.gen = 8;
    sim_net_bridge_event(&test_ws, 7, SIM_BRIDGE_OPEN, NULL, 0);
    if (test_ws.bridge_open) osal_panic("old bridge callback became visible");
    sim_net_bridge_event(&test_ws, 8, SIM_BRIDGE_OPEN, NULL, 0);
    if (!test_ws.bridge_open) osal_panic("current bridge callback was lost");
    test_ws.close_req = true;
    sim_net_bridge_event(&test_ws, 8, SIM_BRIDGE_CLOSED, NULL, 0);
    if (test_ws.dead) osal_panic("closed bridge accepted a late callback");
    test_ws.close_req = false;
    test_ws.state = 2;
    sim_net_bridge_event(&test_ws, 8, SIM_BRIDGE_CLOSED, NULL, 0);
    if (test_ws.dead) osal_panic("finished connection accepted a late callback");
    if (sim_metric_value("bridge.stale_callbacks") != 3) osal_panic("stale callbacks were not counted");
}

void ink_main(void) { vtk_event_after(SIM_MS(1), check_callbacks, NULL); }
