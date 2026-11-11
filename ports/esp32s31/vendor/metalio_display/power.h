/* Private adapter status; public HAL and board interfaces stay unchanged. */
#ifndef INK_S31_VENDOR_PANEL_POWER_H
#define INK_S31_VENDOR_PANEL_POWER_H
#include "metalio_plus/metalio_epd_power.h"
bool ink_panel_off_confirmed(epd_power_t *power);
#endif
