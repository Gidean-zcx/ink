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
/** Airplane mode: AT+CFUN=4 turns the radio off, sending and receiving, the card stays powered (3GPP TS 27.007
 *  §8.2); AT+CFUN=1 turns it on again and waits up to reg_timeout_ms for the card to register. Not kept by the
 *  modem across a restart: the system says it again. */
nt26_result_t nt26_radio(bool on, uint32_t reg_timeout_ms);
const char *nt26_result_name(nt26_result_t r);

/* ---- SMS (text mode, UCS2 so Chinese works) and voice calls (3GPP TS 27.005 / 27.007) ---- */
/** Send a text message (UTF-8). The card's plan must include SMS. */
nt26_result_t nt26_sms_send(const char *number, const char *utf8, uint32_t timeout_ms);
/** Read message `index` (from a +CMTI notification): sender and UTF-8 text. */
nt26_result_t nt26_sms_read(int index, char *number, int number_cap, char *utf8, int text_cap);
nt26_result_t nt26_sms_delete(int index);
nt26_result_t nt26_dial(const char *number); /* voice call (VoLTE) */
nt26_result_t nt26_answer(void);
nt26_result_t nt26_hangup(void);
nt26_result_t nt26_mute(bool on, bool *muted);
nt26_result_t nt26_dtmf(char key);
/* UTF-8 <-> UCS2 hex as AT+CSCS="UCS2" wants it */
int nt26_utf8_to_ucs2_hex(const char *utf8, char *hex, int cap);
int nt26_ucs2_hex_to_utf8(const char *hex, char *utf8, int cap);

#endif
