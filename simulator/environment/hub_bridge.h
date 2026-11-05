/* Host-only loopback WebSocket bridge. No system/app headers depend on this. */
#ifndef INK_HUB_BRIDGE_H
#define INK_HUB_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct sim_hub_bridge sim_hub_bridge_t;
int sim_hub_bridge_configure(const char *url);
sim_hub_bridge_t *sim_hub_bridge_open(void *owner, uint64_t generation);
bool sim_hub_bridge_send(sim_hub_bridge_t *bridge, const void *data, size_t len, bool binary);
/* Consumes the handle without waiting for the host worker. */
void sim_hub_bridge_close(sim_hub_bridge_t *bridge);
void sim_hub_bridge_shutdown(void);

/* Called only by an external-injection callback in the virtual kernel. */
void sim_net_bridge_event(void *owner, uint64_t generation, int event, const void *data, size_t len);
enum { SIM_BRIDGE_OPEN = 1, SIM_BRIDGE_TEXT, SIM_BRIDGE_BINARY, SIM_BRIDGE_CLOSED };
#endif
