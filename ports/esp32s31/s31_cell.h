#ifndef INK_S31_CELL_H
#define INK_S31_CELL_H
#include "ink/hal_net.h"
#ifdef __cplusplus
extern "C" {
#endif
void ink_s31_cell_init(void);
void ink_s31_cell_netif_init(void);
void ink_s31_cell_use(bool on);
void ink_s31_cell_sync(void); /* existing ESP event task */
bool ink_s31_cell_link(hal_link_status_t *out);
bool ink_s31_cell_available(void);
void ink_s31_cell_update(bool registered, int signal_dbm);
void ink_s31_cell_start_data(void);
bool ink_s31_cell_suspend(void);
void ink_s31_cell_resume(void);
void ink_s31_net_changed(void);
#ifdef __cplusplus
}
#endif
#endif
