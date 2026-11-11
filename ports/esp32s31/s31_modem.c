/* AT policy and URC queue; wire protocol belongs to UartEthModem. */
#include <string.h>
#include <stdio.h>
#include "ink/osal.h"
#include "s31_uhci.h"
#include "s31_cell.h"
static osal_queue_t *ink_urcs;
void ink_s31_urc_init(void) {
    if (!ink_urcs) ink_urcs = osal_queue_create(160, 8);
    if (!ink_urcs) osal_panic("cell: no URC memory");
}
/* Deliberately finite: neither arbitrary AT text nor command chaining can reach UART2. */
bool ink_s31_at_allowed(const char *cmd) {
    static const char *const exact[] = {
        "AT", "ATI", "AT+CGMM", "AT+CGMR", "AT+CPIN?", "AT+CEREG?", "AT+CSQ",
        "AT+ECSIMCFG?", "AT+ECNETCFG?", "AT+ECNETDEVCTL?", "AT+CMGF?", "AT+CMGF=?",
        "AT+CSMS=?", "AT+CNMI?", "AT+CIREG?", "AT+CIREG=?", "AT+CLCC", "AT+CMUT?",
        "AT+CFUN?", "AT+CFUN=1", "AT+CFUN=4", "AT+ECNETDEVCTL=2,1,1", "AT+CMGF=0",
        "ATA", "AT+CHUP", "AT+CMUT=0", "AT+CMUT=1",
        /* the card the person picks in Settings (D-128, user 2026-11-11 "手选切换卡吧"): the vendor's switch, nothing else */
        "AT+CFUN=0", "AT+ECSIMCFG=SimSlot,0", "AT+ECSIMCFG=SimSlot,1"
    };
    if (!cmd) return false;
    for (unsigned i = 0; i < sizeof exact / sizeof *exact; i++) if (!strcmp(cmd, exact[i])) return true;
    if (!strncmp(cmd, "AT+CMGS=", 8) || !strncmp(cmd, "AT+CMGR=", 8)) {
        const char *p = cmd + 8;
        unsigned n = 0;
        for (; *p >= '0' && *p <= '9' && n < 5; p++) n++;
        return n && !*p;
    }
    if (!strncmp(cmd, "ATD", 3)) {
        const char *p = cmd + 3;
        if (*p == '+') p++;
        unsigned n = 0;
        for (; *p >= '0' && *p <= '9' && n < 20; p++) n++;
        return n && !strcmp(p, ";");
    }
    return strlen(cmd) == 10 && !strncmp(cmd, "AT+VTS=\"", 8) &&
        strchr("0123456789*#ABCD", cmd[8]) && !strcmp(cmd + 9, "\"");
}

bool ink_s31_at_urc(const char *s) {
    if (!strcmp(s, "ECRDY") || !strcmp(s, "+ECRDY")) {
        ink_s31_cell_update(false, 0);
        s = "+CEREG: 0";
    }
    bool urc = !strcmp(s, "RING") || !strcmp(s, "NO CARRIER") || !strcmp(s, "ECRDY") || !strcmp(s, "+ECRDY") ||
        !strncmp(s, "+CMTI:", 6) || !strncmp(s, "+CLIP:", 6) || !strncmp(s, "+CLCC:", 6) ||
        !strncmp(s, "+CEREG:", 7) || !strncmp(s, "+ECNETDEVCTL:", 13);
    if (urc && ink_urcs && strlen(s) < 160) {
        char item[160] = {0};
        memcpy(item, s, strlen(s));
        osal_queue_send(ink_urcs, item, 0);
    }
    return urc;
}
int hal_modem_urc(char *buf, size_t cap, uint32_t ms) {
    char item[160];
    if (!ink_urcs || !buf || !cap || osal_queue_recv(ink_urcs, item, (int64_t)ms * 1000) != OSAL_OK) return -1;
    size_t n = strlen(item);
    if (n >= cap) return -1;
    memcpy(buf, item, n + 1);
    return (int)n;
}
