/*
 * NT26 SIM selection (see nt26.h). Timeouts and the 500 ms pauses between commands follow the
 * official settings page (settings_network_tab.cc @690a023); unlike it, nothing reboots
 * afterwards: the driver waits for the new card's registration instead.
 */
#include "nt26/nt26.h"

#include <stdio.h>
#include <string.h>

#include "ink/hal_net.h"
#include "ink/osal.h"

#define NT26_CFUN0_TIMEOUT_MS 8000
#define NT26_SLOT_TIMEOUT_MS 5000
#define NT26_CFUN1_TIMEOUT_MS 15000
#define NT26_QUERY_TIMEOUT_MS 5000
#define NT26_STEP_PAUSE_US 500000
#define NT26_REG_POLL_US 500000

static nt26_result_t at(const char *cmd, char *resp, size_t cap, uint32_t timeout_ms) {
    hal_at_result_t r = hal_modem_at(cmd, resp, cap, timeout_ms);
    return r == HAL_AT_OK ? NT26_OK : r == HAL_AT_NO_MODEM ? NT26_NO_MODEM : NT26_AT_FAILED;
}

nt26_result_t nt26_sim_slot(nt26_slot_t *slot) {
    char resp[64];
    nt26_result_t r = at("AT+ECSIMCFG?", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    const char *p = strstr(resp, "\"SimSlot\"");
    int v = -1;
    if (!p || sscanf(p + strlen("\"SimSlot\""), " ,%d", &v) != 1 || (v != 0 && v != 1)) return NT26_AT_FAILED;
    *slot = (nt26_slot_t)v;
    return NT26_OK;
}

nt26_result_t nt26_registered(bool *registered) {
    char resp[64];
    nt26_result_t r = at("AT+CEREG?", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    int n, stat;
    const char *p = strstr(resp, "+CEREG:");
    if (!p || sscanf(p, "+CEREG: %d,%d", &n, &stat) != 2) return NT26_AT_FAILED;
    *registered = stat == 1 || stat == 5;
    return NT26_OK;
}

nt26_result_t nt26_select_sim(nt26_slot_t slot, uint32_t reg_timeout_ms) {
    char resp[64], cmd[40];
    nt26_result_t r = at("AT+CFUN=0", resp, sizeof resp, NT26_CFUN0_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    osal_task_delay_us(NT26_STEP_PAUSE_US);
    snprintf(cmd, sizeof cmd, "AT+ECSIMCFG=SimSlot,%d", (int)slot);
    r = at(cmd, resp, sizeof resp, NT26_SLOT_TIMEOUT_MS);
    if (r != NT26_OK) { /* leave the radio on, on the old card */
        at("AT+CFUN=1", resp, sizeof resp, NT26_CFUN1_TIMEOUT_MS);
        return r;
    }
    osal_task_delay_us(NT26_STEP_PAUSE_US);
    r = at("AT+CFUN=1", resp, sizeof resp, NT26_CFUN1_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    uint64_t deadline = osal_now_us() + (uint64_t)reg_timeout_ms * 1000u;
    for (;;) {
        bool reg = false;
        if (nt26_registered(&reg) == NT26_OK && reg) return NT26_OK;
        if (osal_now_us() >= deadline) return NT26_NOT_REGISTERED;
        osal_task_delay_us(NT26_REG_POLL_US);
    }
}

const char *nt26_result_name(nt26_result_t r) {
    switch (r) {
    case NT26_OK: return "ok";
    case NT26_NOT_REGISTERED: return "not registered";
    case NT26_AT_FAILED: return "AT failed";
    case NT26_NO_MODEM: return "no modem";
    }
    return "?";
}

/* ------------------------------------------------------------- SMS and calls */

int nt26_utf8_to_ucs2_hex(const char *u, char *hex, int cap) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)u;
    while (*p) {
        unsigned cp;
        if (*p < 0x80) cp = *p++;
        else if ((*p & 0xE0) == 0xC0 && p[1]) cp = (unsigned)(*p & 0x1F) << 6 | (p[1] & 0x3F), p += 2;
        else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) cp = (unsigned)(*p & 0x0F) << 12 | (unsigned)(p[1] & 0x3F) << 6 | (p[2] & 0x3F), p += 3;
        else if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3])
            cp = (unsigned)(*p & 0x07) << 18 | (unsigned)(p[1] & 0x3F) << 12 | (unsigned)(p[2] & 0x3F) << 6 | (p[3] & 0x3F), p += 4;
        else return -1;
        unsigned units[2], k = 0;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            units[k++] = 0xD800 | (cp >> 10);
            units[k++] = 0xDC00 | (cp & 0x3FF);
        } else {
            units[k++] = cp;
        }
        for (unsigned i = 0; i < k; i++) {
            if (n + 5 > cap) return -1;
            n += snprintf(hex + n, (size_t)(cap - n), "%04X", units[i]);
        }
    }
    if (n < cap) hex[n] = 0;
    return n;
}

static int hexval(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }

int nt26_ucs2_hex_to_utf8(const char *hex, char *out, int cap) {
    int n = 0;
    size_t len = strlen(hex);
    for (size_t i = 0; i + 3 < len; i += 4) {
        unsigned u = 0;
        for (int k = 0; k < 4; k++) {
            int v = hexval(hex[i + (size_t)k]);
            if (v < 0) return -1;
            u = u << 4 | (unsigned)v;
        }
        unsigned cp = u;
        if (u >= 0xD800 && u < 0xDC00 && i + 7 < len) { /* surrogate pair */
            unsigned lo = 0;
            for (int k = 4; k < 8; k++) lo = lo << 4 | (unsigned)hexval(hex[i + (size_t)k]);
            cp = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
            i += 4;
        }
        char b[4];
        int m = cp < 0x80 ? (b[0] = (char)cp, 1)
              : cp < 0x800 ? (b[0] = (char)(0xC0 | cp >> 6), b[1] = (char)(0x80 | (cp & 0x3F)), 2)
              : cp < 0x10000 ? (b[0] = (char)(0xE0 | cp >> 12), b[1] = (char)(0x80 | (cp >> 6 & 0x3F)), b[2] = (char)(0x80 | (cp & 0x3F)), 3)
                             : (b[0] = (char)(0xF0 | cp >> 18), b[1] = (char)(0x80 | (cp >> 12 & 0x3F)), b[2] = (char)(0x80 | (cp >> 6 & 0x3F)),
                                b[3] = (char)(0x80 | (cp & 0x3F)), 4);
        if (n + m + 1 > cap) return -1;
        memcpy(out + n, b, (size_t)m);
        n += m;
    }
    out[n] = 0;
    return n;
}

static nt26_result_t text_mode_ucs2(void) {
    char resp[64];
    nt26_result_t r = at("AT+CMGF=1", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r == NT26_OK) r = at("AT+CSCS=\"UCS2\"", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    return r;
}

nt26_result_t nt26_sms_send(const char *number, const char *utf8, uint32_t timeout_ms) {
    char num_hex[64], text_hex[1024], cmd[96], resp[64];
    nt26_result_t r = text_mode_ucs2();
    if (r != NT26_OK) return r;
    if (nt26_utf8_to_ucs2_hex(number, num_hex, sizeof num_hex) < 0 || nt26_utf8_to_ucs2_hex(utf8, text_hex, sizeof text_hex) < 0)
        return NT26_AT_FAILED;
    snprintf(cmd, sizeof cmd, "AT+CMGS=\"%s\"", num_hex);
    hal_at_result_t a = hal_modem_at_payload(cmd, text_hex, resp, sizeof resp, timeout_ms);
    return a == HAL_AT_OK ? NT26_OK : a == HAL_AT_NO_MODEM ? NT26_NO_MODEM : NT26_AT_FAILED;
}

nt26_result_t nt26_sms_read(int index, char *number, int number_cap, char *utf8, int text_cap) {
    char cmd[24], resp[1200];
    nt26_result_t r = text_mode_ucs2();
    if (r != NT26_OK) return r;
    snprintf(cmd, sizeof cmd, "AT+CMGR=%d", index);
    r = at(cmd, resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    /* +CMGR: "REC UNREAD","<number hex>",,"<time>"\n<text hex> */
    const char *q1 = strchr(resp, ','), *nl = strchr(resp, '\n');
    if (!q1 || !nl || q1[1] != '"') return NT26_AT_FAILED;
    const char *q2 = strchr(q1 + 2, '"');
    if (!q2) return NT26_AT_FAILED;
    char num_hex[64];
    snprintf(num_hex, sizeof num_hex, "%.*s", (int)(q2 - q1 - 2), q1 + 2);
    if (nt26_ucs2_hex_to_utf8(num_hex, number, number_cap) < 0 || nt26_ucs2_hex_to_utf8(nl + 1, utf8, text_cap) < 0) return NT26_AT_FAILED;
    return NT26_OK;
}

nt26_result_t nt26_sms_delete(int index) {
    char cmd[24], resp[32];
    snprintf(cmd, sizeof cmd, "AT+CMGD=%d", index);
    return at(cmd, resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
}

nt26_result_t nt26_dial(const char *number) {
    char cmd[48], resp[32];
    snprintf(cmd, sizeof cmd, "ATD%s;", number);
    return at(cmd, resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
}

nt26_result_t nt26_answer(void) {
    char resp[32];
    return at("ATA", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
}

nt26_result_t nt26_hangup(void) {
    char resp[32];
    return at("AT+CHUP", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
}
