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

/* Strict UTF-8 decoding: no overlong forms, no surrogates, nothing above U+10FFFF. Returns the
 * bytes used, 0 at the end, -1 on malformed input. */
static int utf8_next(const unsigned char *p, unsigned *cp) {
    if (!p[0]) return 0;
    if (p[0] < 0x80) {
        *cp = p[0];
        return 1;
    }
    int n = (p[0] & 0xE0) == 0xC0 ? 2 : (p[0] & 0xF0) == 0xE0 ? 3 : (p[0] & 0xF8) == 0xF0 ? 4 : 0;
    if (!n) return -1;
    unsigned v = p[0] & (0x7F >> n);
    for (int i = 1; i < n; i++) {
        if ((p[i] & 0xC0) != 0x80) return -1;
        v = v << 6 | (p[i] & 0x3F);
    }
    static const unsigned min_for[5] = {0, 0, 0x80, 0x800, 0x10000};
    if (v < min_for[n] || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF)) return -1;
    *cp = v;
    return n;
}

/* UTF-8 to the hex of UTF-16BE code units, which is what modems take for AT+CSCS="UCS2" (code
 * points above U+FFFF become surrogate pairs; strictly UCS2 has no such pairs, and whether the
 * EC718PM firmware passes them on is not known). -1 on malformed input or a full buffer. */
int nt26_utf8_to_ucs2_hex(const char *u, char *hex, int cap) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)u;
    for (;;) {
        unsigned cp;
        int used = utf8_next(p, &cp);
        if (used == 0) break;
        if (used < 0) return -1;
        p += used;
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
    if (n >= cap) return -1;
    hex[n] = 0;
    return n;
}

static int hexval(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }

static int hex16(const char *h, unsigned *v) {
    unsigned x = 0;
    for (int k = 0; k < 4; k++) {
        int d = hexval(h[k]);
        if (d < 0) return -1;
        x = x << 4 | (unsigned)d;
    }
    *v = x;
    return 0;
}

/* The reverse: hex of UTF-16BE units to UTF-8. -1 if the length is not a multiple of 4, a digit
 * is not hex, a surrogate is unpaired, or the buffer is full. */
int nt26_ucs2_hex_to_utf8(const char *hex, char *out, int cap) {
    size_t len = strlen(hex);
    if (len % 4) return -1;
    int n = 0;
    for (size_t i = 0; i < len; i += 4) {
        unsigned u, cp;
        if (hex16(hex + i, &u)) return -1;
        if (u >= 0xDC00 && u <= 0xDFFF) return -1; /* a low surrogate without its high half */
        if (u >= 0xD800 && u <= 0xDBFF) {
            unsigned lo;
            if (i + 8 > len || hex16(hex + i + 4, &lo) || lo < 0xDC00 || lo > 0xDFFF) return -1;
            cp = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
            i += 4;
        } else {
            cp = u;
        }
        char b[4];
        int m;
        if (cp < 0x80) {
            b[0] = (char)cp;
            m = 1;
        } else if (cp < 0x800) {
            b[0] = (char)(0xC0 | cp >> 6);
            b[1] = (char)(0x80 | (cp & 0x3F));
            m = 2;
        } else if (cp < 0x10000) {
            b[0] = (char)(0xE0 | cp >> 12);
            b[1] = (char)(0x80 | (cp >> 6 & 0x3F));
            b[2] = (char)(0x80 | (cp & 0x3F));
            m = 3;
        } else {
            b[0] = (char)(0xF0 | cp >> 18);
            b[1] = (char)(0x80 | (cp >> 12 & 0x3F));
            b[2] = (char)(0x80 | (cp >> 6 & 0x3F));
            b[3] = (char)(0x80 | (cp & 0x3F));
            m = 4;
        }
        if (n + m + 1 > cap) return -1;
        memcpy(out + n, b, (size_t)m);
        n += m;
    }
    if (n >= cap) return -1;
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
    char num_hex[100], text_hex[1024], cmd[128], resp[64]; /* numbers up to 24 characters, 4 hex digits each */
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

/* TS 27.007; target firmware support still needs its NT26 AT manual. */
nt26_result_t nt26_mute(bool on, bool *muted) {
    char resp[48];
    nt26_result_t r = at(on ? "AT+CMUT=1" : "AT+CMUT=0", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    r = at("AT+CMUT?", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    const char *p = strstr(resp, "+CMUT: ");
    if (r != NT26_OK || !p || (p[7] != '0' && p[7] != '1')) return NT26_AT_FAILED;
    *muted = p[7] == '1';
    return NT26_OK;
}

nt26_result_t nt26_dtmf(char key) {
    char cmd[20], resp[32];
    if (!key || !strchr("0123456789*#ABCD", key)) return NT26_AT_FAILED;
    snprintf(cmd, sizeof cmd, "AT+VTS=\"%c\"", key);
    return at(cmd, resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
}
