/*
 * Cellular HAL on the simulated NT26 (see ink/hal_cellular.h): the project's
 * own driver (drivers/chips/nt26) over the modem's AT channel; events are the
 * modem's unsolicited result codes (+CMTI, +CLIP, NO CARRIER). The device
 * port does the same over its UART.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_cellular.h"
#include "ink/hal_net.h"
#include "nt26/nt26.h"

bool hal_cell_status(hal_cell_status_t *out) {
    *out = (hal_cell_status_t){0};
    nt26_slot_t slot;
    bool reg = false;
    if (nt26_sim_slot(&slot) != NT26_OK || nt26_registration(&reg, &out->roaming) != NT26_OK) return false;
    out->slot = slot == NT26_INTERNAL_CARD ? HAL_CELL_SLOT_INTERNAL : HAL_CELL_SLOT_EXTERNAL;
    out->registered = reg;
    char response[64]; int csq = 99;
    if (hal_modem_at("AT+CSQ", response, sizeof response, 500) == HAL_AT_OK) sscanf(response, "+CSQ: %d", &csq);
    out->signal_dbm = csq >= 0 && csq <= 31 ? -113 + 2 * csq : 0;
    out->sim_missing = hal_modem_at("AT+CPIN?", response, sizeof response, 1000) == HAL_AT_ERROR;
    return true;
}
int hal_cell_apn(const hal_cell_apn_t *a) { return nt26_apn(a->name, a->user, a->password) == NT26_OK ? 0 : -1; }

int hal_cell_select_sim(hal_cell_slot_t slot, uint32_t reg_timeout_ms) {
    nt26_result_t r = nt26_select_sim(slot == HAL_CELL_SLOT_INTERNAL ? NT26_INTERNAL_CARD : NT26_EXTERNAL_SLOT, reg_timeout_ms);
    return r == NT26_OK ? 0 : r == NT26_NOT_REGISTERED ? 1 : -1;
}

int hal_cell_radio(bool on, uint32_t reg_timeout_ms) {
    nt26_result_t r = nt26_radio(on, reg_timeout_ms);
    return r == NT26_OK ? 0 : r == NT26_NOT_REGISTERED ? 1 : -1;
}

int hal_cell_sms_parts(const char *utf8) { return nt26_sms_split(utf8, NULL, 0, NULL); }

int hal_cell_sms_send(const char *number, const char *utf8, uint32_t timeout_ms, int *sent) {
    nt26_result_t r = nt26_sms_send_parts(number, utf8, timeout_ms, sent);
    return r == NT26_OK ? 0 : r == NT26_NOT_SENT ? 1 : -1;
}

int hal_cell_sms_take(int index, hal_cell_sms_t *m) {
    int concat[3];
    if (nt26_sms_read_part(index, m->number, sizeof m->number, m->text, sizeof m->text, concat) != NT26_OK) return -1;
    m->ref = concat[0], m->total = concat[1], m->seq = concat[2];
    nt26_sms_delete(index);
    return (int)strlen(m->text);
}

int hal_cell_dial(const char *number) { return nt26_dial(number) == NT26_OK ? 0 : -1; }
int hal_cell_answer(void) { return nt26_answer() == NT26_OK ? 0 : -1; }
int hal_cell_hangup(void) { return nt26_hangup() == NT26_OK ? 0 : -1; }
int hal_cell_mute(bool on, bool *muted) { return nt26_mute(on, muted) == NT26_OK ? 0 : -1; }
int hal_cell_dtmf(char key) { return nt26_dtmf(key) == NT26_OK ? 0 : -1; }

int hal_cell_wait_event(hal_cell_event_t *ev, uint32_t timeout_ms) {
    char line[160];
    if (hal_modem_urc(line, sizeof line, timeout_ms) < 0) return 0;
    memset(ev, 0, sizeof *ev);
    if (!strncmp(line, "+CEREG:", 7)) { ev->type = HAL_CELL_EV_REG; return 1; }
    if (!strncmp(line, "+CMTI:", 6)) { /* +CMTI: "SM",<index> */
        const char *comma = strchr(line, ',');
        if (!comma) return 0;
        ev->type = HAL_CELL_EV_SMS;
        ev->sms_index = atoi(comma + 1);
        return 1;
    }
    if (!strncmp(line, "+CLIP:", 6)) { /* +CLIP: "<number>",129 (follows each RING) */
        const char *q = strchr(line, '"'), *e = q ? strchr(q + 1, '"') : NULL;
        ev->type = HAL_CELL_EV_RING;
        if (q && e) snprintf(ev->number, sizeof ev->number, "%.*s", (int)(e - q - 1), q + 1);
        return 1;
    }
    if (!strcmp(line, "NO CARRIER")) {
        ev->type = HAL_CELL_EV_CALL_ENDED;
        return 1;
    }
    int index, direction, status;
    if (sscanf(line, "+CLCC: %d,%d,%d", &index, &direction, &status) == 3 && direction == 0 && status == 0) {
        ev->type = HAL_CELL_EV_CALL_ACTIVE;
        return 1;
    }
    return 0; /* RING itself (the +CLIP that follows carries the number), registration, others */
}
