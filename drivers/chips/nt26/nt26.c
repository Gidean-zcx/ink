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
    char resp[512]; /* the NT26 lists every SIM setting (about 330 bytes on the device, 2026-11-11); 64 overflowed into ERROR */
    nt26_result_t r = at("AT+ECSIMCFG?", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    const char *p = strstr(resp, "\"SimSlot\"");
    int v = -1;
    if (!p || sscanf(p + strlen("\"SimSlot\""), " ,%d", &v) != 1 || (v != 0 && v != 1)) return NT26_AT_FAILED;
    *slot = (nt26_slot_t)v;
    return NT26_OK;
}

nt26_result_t nt26_registered(bool *registered) { return nt26_registration(registered, NULL); }
nt26_result_t nt26_registration(bool *registered, bool *roaming) {
    char resp[64];
    nt26_result_t r = at("AT+CEREG?", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    int n, stat;
    const char *p = strstr(resp, "+CEREG:");
    if (!p || sscanf(p, "+CEREG: %d,%d", &n, &stat) != 2) return NT26_AT_FAILED;
    *registered = stat == 1 || stat == 5;
    if (roaming) *roaming = stat == 5;
    return NT26_OK;
}

/* TS 27.007 §10.1.1 +CGDCONT, §10.1.31 +CGAUTH (PAP), §10.1.10 +CGACT. */
nt26_result_t nt26_apn(const char *name, const char *user, const char *password) {
    const char *parts[] = {name, user, password};
    for (unsigned i = 0; i < sizeof parts / sizeof *parts; i++)
        for (const unsigned char *p = (const unsigned char *)parts[i]; *p; p++)
            if (*p < 32 || *p > 126 || *p == '"' || *p == '\\') return NT26_AT_FAILED;
    char cmd[180], resp[64];
    nt26_result_t r = at("AT+CGACT=0,1", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    snprintf(cmd, sizeof cmd, "AT+CGDCONT=1,\"IP\",\"%.100s\"", name);
    r = at(cmd, resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r == NT26_OK) {
        snprintf(cmd, sizeof cmd, "AT+CGAUTH=1,%d,\"%.64s\",\"%.64s\"", user[0] || password[0] ? 1 : 0, user, password);
        r = at(cmd, resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    }
    memset(cmd, 0, sizeof cmd);
    nt26_result_t resumed = at("AT+CGACT=1,1", resp, sizeof resp, NT26_CFUN1_TIMEOUT_MS);
    return r == NT26_OK ? resumed : r;
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
    return nt26_radio(true, reg_timeout_ms);
}

nt26_result_t nt26_radio(bool on, uint32_t reg_timeout_ms) {
    char resp[64];
    nt26_result_t r = at(on ? "AT+CFUN=1" : "AT+CFUN=4", resp, sizeof resp, on ? NT26_CFUN1_TIMEOUT_MS : NT26_CFUN0_TIMEOUT_MS);
    if (r != NT26_OK || !on) return r;
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
    case NT26_NOT_SENT: return "not sent";
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

/* One code point as UTF-8 at out[*n], room left for the NUL. -1 when it does not fit. */
static int utf8_put(char *out, int cap, int *n, unsigned cp) {
    int m = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
    if (*n + m + 1 > cap) return -1;
    char *b = out + *n;
    if (m == 1) b[0] = (char)cp;
    else {
        b[0] = (char)((0xF00 >> m) | (cp >> (6 * (m - 1))));
        for (int k = 1; k < m; k++) b[k] = (char)(0x80 | (cp >> (6 * (m - 1 - k)) & 0x3F));
    }
    *n += m;
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
        if (utf8_put(out, cap, &n, cp)) return -1;
    }
    if (n >= cap) return -1;
    out[n] = 0;
    return n;
}

/* ---- PDU mode: 3GPP TS 23.040 §9.2 (TPDUs), TS 23.038 §5, §6.2.1 (alphabets), TS 27.005 §3.4.3 +CMGR, §3.5.1 +CMGS */

/* The GSM 7-bit default alphabet: the code point of each septet (0x1B escapes to the extension table, never matched). */
static unsigned gsm7_cp(unsigned s) {
    static const uint16_t LOW[32] = {0x40, 0xA3, 0x24, 0xA5, 0xE8, 0xE9, 0xF9, 0xEC, 0xF2, 0xC7, 0x0A, 0xD8, 0xF8, 0x0D, 0xC5, 0xE5,
        0x394, 0x5F, 0x3A6, 0x393, 0x39B, 0x3A9, 0x3A0, 0x3A8, 0x3A3, 0x398, 0x39E, 0xFFFF, 0xC6, 0xE6, 0xDF, 0xC9};
    static const char HIGH[] = "\x24\x40\x5B\x5C\x5D\x5E\x5F\x60\x7B\x7C\x7D\x7E\x7F";
    static const uint16_t HIGH_CP[] = {0xA4, 0xA1, 0xC4, 0xD6, 0xD1, 0xDC, 0xA7, 0xBF, 0xE4, 0xF6, 0xF1, 0xFC, 0xE0};
    const char *p = s >= 32 ? strchr(HIGH, (int)s) : NULL;
    return s < 32 ? LOW[s] : p ? HIGH_CP[p - HIGH] : s;
}
static int gsm7_septet(unsigned cp) {
    for (unsigned s = 0; s < 128; s++) if (gsm7_cp(s) == cp) return (int)s;
    return -1;
}
/* Septet k of packed user data, least significant bit first (TS 23.038 §6.1.2.1.1). */
static unsigned get_septet(const uint8_t *ud, int k) {
    int bit = 7 * k;
    unsigned v = ud[bit / 8] >> (bit % 8);
    if (bit % 8 > 1) v |= (unsigned)ud[bit / 8 + 1] << (8 - bit % 8);
    return v & 0x7F;
}
static void put_septet(uint8_t *ud, int k, unsigned v) {
    int bit = 7 * k;
    ud[bit / 8] |= (uint8_t)(v << (bit % 8));
    if (bit % 8 > 1) ud[bit / 8 + 1] |= (uint8_t)(v >> (8 - bit % 8));
}
/* Septets [from, to) as UTF-8; the escape takes the next one from the extension table (TS 23.038 §6.2.1.1). */
static int gsm7_to_utf8(const uint8_t *ud, int from, int to, char *out, int cap) {
    static const char EXT[] = "\x0A\x14\x28\x29\x2F\x3C\x3D\x3E\x40\x65";
    static const uint16_t EXT_CP[] = {0x0C, '^', '{', '}', '\\', '[', '~', ']', '|', 0x20AC};
    int n = 0;
    for (int k = from; k < to; k++) {
        unsigned s = get_septet(ud, k), cp = gsm7_cp(s);
        if (s == 0x1B && k + 1 < to) {
            const char *e = strchr(EXT, (int)(s = get_septet(ud, ++k)));
            cp = e && s ? EXT_CP[e - EXT] : gsm7_cp(s);
        }
        if (cp != 0xFFFF && utf8_put(out, cap, &n, cp)) return -1; /* an escape at the end: nothing */
    }
    out[n] = 0;
    return n;
}

int nt26_sms_split(const char *utf8, int *ends, int max, bool *gsm7) {
    const unsigned char *p = (const unsigned char *)utf8;
    unsigned cp;
    int used, total = 0, parts = 0, in_part = 0;
    bool g = true;
    for (const unsigned char *q = p; (used = utf8_next(q, &cp)) != 0; q += used) {
        if (used < 0) return -1;
        g = g && gsm7_septet(cp) >= 0;
        total += cp >= 0x10000 ? 2 : 1; /* a surrogate pair */
    }
    int limit = g ? 160 : 70, per_part = g ? 153 : 67; /* a concatenated part gives 6 octets to its header */
    for (const unsigned char *q = p; total > limit && (used = utf8_next(q, &cp)) != 0; q += used) {
        int units = g || cp < 0x10000 ? 1 : 2; /* a pair is never split */
        if (in_part + units > per_part) {
            if (parts < max) ends[parts] = (int)(q - p);
            parts++;
            in_part = 0;
        }
        in_part += units;
    }
    if (parts < max) ends[parts] = (int)strlen(utf8);
    if (gsm7) *gsm7 = g;
    return parts + 1;
}

/* One SMS-SUBMIT (TS 23.040 §9.2.2.2), no validity period, the modem numbers it (TP-MR 0); a part of several carries
 * the concatenation header with an 8-bit reference (§9.2.3.24.1). TPDU octets into b. */
static int submit_pdu(uint8_t *b, const char *number, const char *text, int len, bool g, int ref, int total, int seq) {
    const char *d = number + (number[0] == '+');
    int n = 0, digits = (int)strlen(d), hdr = total > 1 ? 6 : 0;
    b[n++] = total > 1 ? 0x41 : 0x01; /* SMS-SUBMIT, TP-UDHI */
    b[n++] = 0;
    b[n++] = (uint8_t)digits;
    b[n++] = number[0] == '+' ? 0x91 : 0x81; /* international or unknown, ISDN numbering (§9.1.2.5) */
    for (int i = 0; i < digits; i += 2) b[n++] = (uint8_t)((d[i] - '0') | (i + 1 < digits ? d[i + 1] - '0' : 0xF) << 4);
    b[n++] = 0;                   /* TP-PID */
    b[n++] = g ? 0x00 : 0x08;     /* TP-DCS: GSM 7-bit or UCS2 (TS 23.038 §4) */
    uint8_t *udl = &b[n++], *ud = &b[n];
    memset(ud, 0, 140);
    if (hdr) memcpy(ud, (uint8_t[]){5, 0, 3, (uint8_t)ref, (uint8_t)total, (uint8_t)seq}, 6);
    int k = g ? (hdr * 8 + 6) / 7 : hdr; /* septets (after the header and its fill bits) or octets */
    unsigned cp;
    for (int i = 0, used; i < len; i += used) {
        used = utf8_next((const unsigned char *)text + i, &cp);
        if (g) put_septet(ud, k++, (unsigned)gsm7_septet(cp));
        else {
            unsigned u[2] = {cp, 0}, m = cp >= 0x10000 ? 2 : 1; /* UTF-16BE, as UCS2 above U+FFFF has no units */
            if (m == 2) u[0] = 0xD800 | ((cp - 0x10000) >> 10), u[1] = 0xDC00 | ((cp - 0x10000) & 0x3FF);
            for (unsigned j = 0; j < m; j++) ud[k++] = (uint8_t)(u[j] >> 8), ud[k++] = (uint8_t)u[j];
        }
    }
    *udl = (uint8_t)k;
    return n + (g ? (k * 7 + 7) / 8 : k);
}

nt26_result_t nt26_sms_send_parts(const char *number, const char *utf8, uint32_t timeout_ms, int *sent) {
    int ends[NT26_SMS_PARTS_MAX], start = 0, done = 0;
    bool g;
    int parts = nt26_sms_split(utf8, ends, NT26_SMS_PARTS_MAX, &g);
    if (sent) *sent = 0;
    if (parts < 1 || parts > NT26_SMS_PARTS_MAX || strlen(number) > 21) return NT26_AT_FAILED;
    char resp[64], cmd[24];
    nt26_result_t r = at("AT+CMGF=0", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    if (r != NT26_OK) return r;
    static uint8_t ref; /* one per message, so the receiver can tell them apart; the first after a restart from the clock */
    ref = ref ? (uint8_t)(ref + 1) : (uint8_t)(osal_now_us() | 1);
    uint64_t deadline = osal_now_us() + (uint64_t)timeout_ms * 1000u;
    for (; done < parts; start = ends[done++]) {
        uint8_t b[176];
        char hex[2 * sizeof b + 3] = "00"; /* the SMSC address from the card */
        int len = submit_pdu(b, number, utf8 + start, ends[done] - start, g, ref, parts, done + 1);
        for (int i = 0; i < len; i++) snprintf(hex + 2 + 2 * i, 3, "%02X", b[i]);
        uint64_t now = osal_now_us();
        if (now + NT26_SMS_MIN_MS * 1000ull > deadline) return NT26_NOT_SENT;
        uint64_t left_ms = (deadline - now) / 1000u;
        snprintf(cmd, sizeof cmd, "AT+CMGS=%d", len); /* TPDU octets, the SMSC address not counted */
        hal_at_result_t a = hal_modem_at_payload(cmd, hex, resp, sizeof resp, left_ms < NT26_SMS_PART_MS ? (uint32_t)left_ms : NT26_SMS_PART_MS);
        if (a != HAL_AT_OK) return a == HAL_AT_NO_MODEM ? NT26_NO_MODEM : NT26_AT_FAILED;
        if (sent) *sent = done + 1;
    }
    return NT26_OK;
}

nt26_result_t nt26_sms_send(const char *number, const char *utf8, uint32_t timeout_ms) { return nt26_sms_send_parts(number, utf8, timeout_ms, NULL); }

/* An SMS-DELIVER (TS 23.040 §9.2.2.1) from AT+CMGR in PDU mode: "+CMGR: <stat>,[<alpha>],<length>\n<pdu>". */
nt26_result_t nt26_sms_read_part(int index, char *number, int number_cap, char *utf8, int text_cap, int concat[3]) {
    char cmd[24], resp[420];
    uint8_t b[180];
    nt26_result_t r = at("AT+CMGF=0", resp, sizeof resp, NT26_QUERY_TIMEOUT_MS);
    snprintf(cmd, sizeof cmd, "AT+CMGR=%d", index);
    if (r != NT26_OK || (r = at(cmd, resp, sizeof resp, NT26_QUERY_TIMEOUT_MS)) != NT26_OK) return r;
    const char *hex = strchr(resp, '\n');
    int n = 0, i, dn = 0;
    for (hex = hex ? hex + 1 : ""; hexval(hex[2 * n]) >= 0 && hexval(hex[2 * n + 1]) >= 0 && n < (int)sizeof b; n++)
        b[n] = (uint8_t)(hexval(hex[2 * n]) << 4 | hexval(hex[2 * n + 1]));
    if (n < 1 || (i = 1 + b[0]) + 3 > n || (b[i] & 3) != 0) return NT26_AT_FAILED; /* SMS-DELIVER only */
    bool udhi = b[i] & 0x40;
    int digits = b[i + 1], toa = b[i + 2];
    i += 3;
    if (i + (digits + 1) / 2 + 10 > n || number_cap < 2) return NT26_AT_FAILED;
    if ((toa & 0x70) == 0x50) { /* alphanumeric: GSM 7-bit (§9.1.2.5) */
        if (gsm7_to_utf8(b + i, 0, digits * 4 / 7, number, number_cap) < 0) return NT26_AT_FAILED;
    } else {
        if ((toa & 0x70) == 0x10) number[dn++] = '+';
        for (int k = 0; k < digits && dn + 1 < number_cap; k++) {
            int v = b[i + k / 2] >> (k % 2 * 4) & 0xF;
            number[dn++] = v < 10 ? (char)('0' + v) : "*#abc?"[v - 10];
        }
        number[dn] = 0;
    }
    i += (digits + 1) / 2;
    int dcs = b[i + 1], udl = b[i + 9], hdr = 0;
    const uint8_t *ud = b + i + 10;
    int ud_bytes = n - i - 10, group = dcs >> 4;
    int alphabet = group == 0xF ? (dcs >> 2 & 1) : group == 0xE ? 2 : group >= 0xC ? 0 : (dcs >> 2 & 3); /* TS 23.038 §4 */
    concat[0] = 0, concat[1] = 1, concat[2] = 1;
    if (udhi && ud_bytes > 0 && (hdr = ud[0] + 1) <= ud_bytes) {
        for (int k = 1; k + 1 < hdr && k + 2 + ud[k + 1] <= hdr; k += 2 + ud[k + 1]) { /* IEI 0x00, 0x08 (§9.2.3.24.1, .8) */
            const uint8_t *e = ud + k + 2;
            if (ud[k] == 0x00 && ud[k + 1] == 3) concat[0] = e[0], concat[1] = e[1], concat[2] = e[2];
            if (ud[k] == 0x08 && ud[k + 1] == 4) concat[0] = e[0] << 8 | e[1], concat[1] = e[2], concat[2] = e[3];
        }
        if (concat[2] < 1 || concat[2] > concat[1]) concat[1] = concat[2] = 1; /* ignored, as the standard says */
    }
    if (alphabet == 0) {
        if ((udl * 7 + 7) / 8 > ud_bytes || gsm7_to_utf8(ud, (hdr * 8 + 6) / 7, udl, utf8, text_cap) < 0) return NT26_AT_FAILED;
    } else if (alphabet == 2 && udl <= ud_bytes && hdr <= udl) {
        char units[2 * 140 + 1];
        snprintf(units, sizeof units, "%.*s", 2 * (udl - hdr), hex + 2 * (ud - b + hdr));
        if (nt26_ucs2_hex_to_utf8(units, utf8, text_cap) < 0) return NT26_AT_FAILED;
    } else return NT26_AT_FAILED; /* 8-bit data is not a text */
    return NT26_OK;
}

nt26_result_t nt26_sms_read(int index, char *number, int number_cap, char *utf8, int text_cap) {
    int concat[3];
    return nt26_sms_read_part(index, number, number_cap, utf8, text_cap, concat);
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
