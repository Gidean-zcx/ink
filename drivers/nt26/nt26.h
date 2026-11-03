/*
 * NT26 cellular modem (EC718PM): SIM card selection over the modem's AT channel (hal_modem_at).
 *
 * The Plus has an external card slot (SimSlot 0) and an internal card (SimSlot 1); the slot is a
 * modem setting that survives reboots. Switching is done online, without restarting the device
 * (R-044): the same commands as the official settings page (settings_network_tab.cc @690a023:
 * AT+CFUN=0, AT+ECSIMCFG=SimSlot,n, AT+CFUN=1), then wait for the new card to register.
 */
#ifndef INK_DRIVERS_NT26_H
#define INK_DRIVERS_NT26_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    NT26_EXTERNAL_SLOT = 0,
    NT26_INTERNAL_CARD = 1,
} nt26_slot_t;

typedef enum {
    NT26_OK = 0,
    NT26_NOT_REGISTERED = 1, /* the switch went through but the card did not register in time */
    NT26_AT_FAILED = -1,     /* a command failed or timed out (the radio is turned back on) */
    NT26_NO_MODEM = -2,
} nt26_result_t;

/** The modem's slot setting (AT+ECSIMCFG?). */
nt26_result_t nt26_sim_slot(nt26_slot_t *slot);
/** Registered on the network with the card in use (AT+CEREG? stat 1 home or 5 roaming). */
nt26_result_t nt26_registered(bool *registered);
/** Switch to the card in `slot` and wait up to reg_timeout_ms for it to register. The device keeps
 *  running; LTE is down for the few seconds of the switch. */
nt26_result_t nt26_select_sim(nt26_slot_t slot, uint32_t reg_timeout_ms);
const char *nt26_result_name(nt26_result_t r);

#endif
