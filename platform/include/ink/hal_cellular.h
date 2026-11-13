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
    int signal_dbm;       /* CSQ RSSI, 0 when unknown */
    bool sim_missing;     /* modem explicitly reports no SIM */
    bool config_readonly; /* SIM selection and APN writes are unavailable */
    bool registered;      /* on the network with the card in use */
    bool roaming;         /* +CEREG stat 5 */
    hal_cell_slot_t slot; /* the card in use */
} hal_cell_status_t;

typedef enum {
    HAL_CELL_EV_SMS = 1,    /* a text message is waiting: `sms_index` */
    HAL_CELL_EV_RING,       /* an incoming call rings (repeats while it rings): `number` */
    HAL_CELL_EV_CALL_ENDED, /* the call ended or the caller gave up */
    HAL_CELL_EV_CALL_ACTIVE, /* the outgoing call was answered */
    HAL_CELL_EV_REG,        /* registration changed: query status */
} hal_cell_ev_type_t;

/* Why HAL_CELL_EV_CALL_ENDED came: the modem's own words (LilyPhone peri_modem.cpp:508-510, 615-616). */
typedef enum {
    HAL_CELL_END_HANGUP = 0, /* NO CARRIER: either side hung up, or the call failed for a reason not given */
    HAL_CELL_END_BUSY,       /* BUSY: the far end is busy or rejected the call */
    HAL_CELL_END_NO_ANSWER,  /* NO ANSWER: nobody answered */
} hal_cell_end_t;

typedef struct {
    hal_cell_ev_type_t type;
    int sms_index;
    char number[24];
    hal_cell_end_t end; /* HAL_CELL_EV_CALL_ENDED */
} hal_cell_event_t;


/** False when there is no modem, or it does not answer. Task context. */
bool hal_cell_status(hal_cell_status_t *out);
typedef struct { char name[101], user[65], password[65]; } hal_cell_apn_t; /* empty name: subscription default */
int hal_cell_apn(const hal_cell_apn_t *apn); /* reconnect only the data context; task context */
/** Use another card, without restarting the modem. 0, 1 (switched but not registered in time), < 0 failed. */
int hal_cell_select_sim(hal_cell_slot_t slot, uint32_t reg_timeout_ms);
/** Airplane mode: the radio off (no service, no calls, no messages) or on again, waiting up to reg_timeout_ms for the
 *  card to register. 0, 1 (on but not registered in time), < 0 failed. Task context, seconds. */
int hal_cell_radio(bool on, uint32_t reg_timeout_ms);
/** A text message as it came: a whole one (total 1), or part `seq` (1-based) of `total` with reference `ref` (concatenated
 *  messages, 3GPP TS 23.040 §9.2.3.24.1). One part holds up to 160 characters. */
typedef struct {
    char number[24];
    char text[340];
    int ref, total, seq;
} hal_cell_sms_t;

/** The parts (each billed) a text goes as, < 0 if it is not UTF-8. */
int hal_cell_sms_parts(const char *utf8);
/** Send a text message (UTF-8), in parts when long. 0: all sent. Otherwise *sent parts went, and the next one failed
 *  with the result unknown (< 0: refused, timed out, no modem) or was not tried (1: no time left); none after it
 *  was tried. Task context, seconds per part. */
int hal_cell_sms_send(const char *number, const char *utf8, uint32_t timeout_ms, int *sent);
/** Read a waiting message or part and remove it from the modem's store. Text length, or < 0. */
int hal_cell_sms_take(int index, hal_cell_sms_t *out);
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
