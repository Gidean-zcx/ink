/*
 * Cellular (4G): text messages, voice calls and which SIM card is used, for
 * the system's communication service. How the modem does it (AT commands,
 * the steps of an online card switch, R-044) is driver code below this
 * interface; the system never sends AT commands itself.
 */
#ifndef INK_HAL_CELLULAR_H
#define INK_HAL_CELLULAR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { HAL_CELL_SLOT_EXTERNAL = 0, HAL_CELL_SLOT_INTERNAL = 1 } hal_cell_slot_t;

typedef struct {
    bool registered;      /* on the network with the card in use */
    hal_cell_slot_t slot; /* the card in use */
} hal_cell_status_t;

typedef enum {
    HAL_CELL_EV_SMS = 1,    /* a text message is waiting: `sms_index` */
    HAL_CELL_EV_RING,       /* an incoming call rings (repeats while it rings): `number` */
    HAL_CELL_EV_CALL_ENDED, /* the call ended or the caller gave up */
    HAL_CELL_EV_CALL_ACTIVE, /* the outgoing call was answered */
} hal_cell_ev_type_t;

typedef struct {
    hal_cell_ev_type_t type;
    int sms_index;
    char number[24];
} hal_cell_event_t;

/** False when there is no modem, or it does not answer. Task context. */
bool hal_cell_status(hal_cell_status_t *out);
/** Use another card, without restarting the modem. 0, 1 (switched but not registered in time), < 0 failed. */
int hal_cell_select_sim(hal_cell_slot_t slot, uint32_t reg_timeout_ms);
/** Airplane mode: the radio off (no service, no calls, no messages) or on again, waiting up to reg_timeout_ms for the
 *  card to register. 0, 1 (on but not registered in time), < 0 failed. Task context, seconds. */
int hal_cell_radio(bool on, uint32_t reg_timeout_ms);
/** Send a text message (UTF-8). 0, or < 0 (no service, refused, timed out). Task context, seconds. */
int hal_cell_sms_send(const char *number, const char *utf8, uint32_t timeout_ms);
/** Read a waiting message and remove it from the modem's store. Text length, or < 0. */
int hal_cell_sms_take(int index, char *number, int number_cap, char *utf8, int text_cap);
int hal_cell_dial(const char *number);
int hal_cell_answer(void);
int hal_cell_hangup(void);
int hal_cell_mute(bool on, bool *muted); /* Read back the modem's actual mute state. */
int hal_cell_dtmf(char key);
/** The next modem event (one reader). 1 got one, 0 none within the timeout. Task context. */
int hal_cell_wait_event(hal_cell_event_t *ev, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
#endif
