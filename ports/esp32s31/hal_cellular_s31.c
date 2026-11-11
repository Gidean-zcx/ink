/*
 * Cellular HAL on the S31 NT26 (see ink/hal_cellular.h): the project's
 * own driver (drivers/chips/nt26) over the modem's AT channel; events are the
 * modem's unsolicited result codes (+CMTI, +CLIP, NO CARRIER). The framed UART channel enforces the device command allowlist.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_cellular.h"
#include "s31_bh1098.h"
#include "ink/hal_device.h"
#include "ink/hal_net.h"
#include "nt26/nt26.h"
#include "ink/osal.h"
#include "s31_cell.h"

/* Any run of ten or more digits becomes '#': no IMEI, IMSI or ICCID reaches the log. */
static void mask_ids(char *s) {
    for (char *p = s; *p;) {
        char *q = p;
        while (*q >= '0' && *q <= '9') q++;
        if (q - p >= 10) memset(p, '#', (size_t)(q - p));
        p = q > p ? q : p + 1;
    }
}

/* Once per boot, when the AT channel first answers: what the modem is and what it can do, by read-only queries from the
 * allowlist (trial 2026-11-11: can it send SMS, can it call over VoLTE). Not ATI: some modems put the IMEI in it. */
static void log_modem_once(void) {
    static bool done;
    if (done) return;
    done = true;
    static const char *const QUERIES[] = {"AT+CGMM", "AT+CGMR", "AT+CFUN?", "AT+CPIN?", "AT+CEREG?", "AT+CSQ", "AT+ECSIMCFG?",
                                          "AT+CSMS=?", "AT+CMGF=?", "AT+CNMI?", "AT+CIREG?", "AT+CIREG=?"};
    for (size_t i = 0; i < sizeof QUERIES / sizeof *QUERIES; i++) {
        char response[512] = "", line[200];
        hal_at_result_t r = hal_modem_at(QUERIES[i], response, sizeof response, 1000);
        for (char *p = response; *p; p++) if (*p == '\r' || *p == '\n') *p = ' ';
        if (!strncmp(QUERIES[i], "AT+CEREG", 8)) { /* keep <n>,<stat>: the area code and cell id say where the device is */
            char *c = strchr(response, ','); c = c ? strchr(c + 1, ',') : NULL;
            if (c) *c = 0;
        }
        mask_ids(response);
        snprintf(line, sizeof line, "cell: %s -> %s %.150s", QUERIES[i],
                 r == HAL_AT_OK ? "OK" : r == HAL_AT_ERROR ? "ERROR" : "no answer", response);
        hal_log_write(line);
    }
}

bool hal_cell_status(hal_cell_status_t *out) {
    *out = (hal_cell_status_t){.config_readonly = true};
    nt26_slot_t slot;
    if (nt26_sim_slot(&slot) != NT26_OK || nt26_registration(&out->registered, &out->roaming) != NT26_OK) return false;
    log_modem_once();
    out->slot = slot == NT26_INTERNAL_CARD ? HAL_CELL_SLOT_INTERNAL : HAL_CELL_SLOT_EXTERNAL;
    char response[96];
    hal_at_result_t r = hal_modem_at("AT+CPIN?", response, sizeof response, 1000);
    out->sim_missing = r == HAL_AT_ERROR && strstr(response, "+CME ERROR: 10") != NULL;
    int csq = 99;
    if (hal_modem_at("AT+CSQ", response, sizeof response, 500) == HAL_AT_OK) {
        const char *p = strstr(response, "+CSQ:");
        if (p) sscanf(p, "+CSQ: %d", &csq);
    }
    out->signal_dbm = csq >= 0 && csq <= 31 ? -113 + 2 * csq : 0;
    ink_s31_cell_update(out->registered, out->signal_dbm);
    ink_s31_cell_start_data();
    return true;
}
int hal_cell_apn(const hal_cell_apn_t *a) { (void)a; return -1; }
int hal_cell_select_sim(hal_cell_slot_t slot, uint32_t ms) { (void)slot; (void)ms; return -1; }

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
    /* Device storage is read-only: leave the modem copy in place. */
    return (int)strlen(m->text);
}

static int ink_call_result(nt26_result_t rc, bool on) {
    if (rc != NT26_OK) return -1;
    ink_s31_bh_call(on);
    return 0;
}
int hal_cell_dial(const char *number) { return ink_call_result(nt26_dial(number), true); }
int hal_cell_answer(void) { return ink_call_result(nt26_answer(), true); }
int hal_cell_hangup(void) { return ink_call_result(nt26_hangup(), false); }
int hal_cell_mute(bool on, bool *muted) { return nt26_mute(on, muted) == NT26_OK ? 0 : -1; }
int hal_cell_dtmf(char key) { return nt26_dtmf(key) == NT26_OK ? 0 : -1; }

int hal_cell_wait_event(hal_cell_event_t *ev, uint32_t timeout_ms) {
    static uint64_t next_status;
    uint64_t now = osal_now_us();
    if (now >= next_status) {
        next_status = now + 30000000;
        *ev = (hal_cell_event_t){.type = HAL_CELL_EV_REG};
        return 1; /* existing comm task refreshes registration and signal */
    }
    char line[160];
    uint32_t left = (uint32_t)((next_status - now + 999) / 1000);
    if (hal_modem_urc(line, sizeof line, timeout_ms < left ? timeout_ms : left) < 0) return 0;
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
        ink_s31_bh_call(false);
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
