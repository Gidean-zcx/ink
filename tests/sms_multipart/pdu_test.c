/* The NT26 driver's PDU mode against fixed vectors: what goes into AT+CMGS and what comes out of AT+CMGR.
 * The two plain vectors are the classic examples of the PDU format (SMS-SUBMIT "hellohello" to +46708251358 and
 * SMS-DELIVER "How are you?" from +31641600986), here without the validity period; the concatenated ones are put
 * together by hand from 3GPP TS 23.040 §9.2.3.24.1 (header 05 00 03 / 06 08 04, the fill bit before GSM 7-bit text). */
#include <stdio.h>
#include <string.h>

#include "ink/hal_net.h"
#include "ink/osal.h"
#include "nt26/nt26.h"

static char cmds[16][32], payloads[16][400], reply[600];
static int ncmd, fail_cmgs, cmgs;
static uint64_t now_us;
static int failures;

#define EXPECT(c, ...) do { if (!(c)) { failures++; printf("FAIL %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

hal_at_result_t hal_modem_at_payload(const char *cmd, const char *payload, char *resp, size_t cap, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (ncmd < 16) snprintf(cmds[ncmd], sizeof cmds[0], "%s", cmd), snprintf(payloads[ncmd], sizeof payloads[0], "%s", payload ? payload : "");
    ncmd++;
    now_us += 2000000; /* each command takes the network two seconds */
    if (!strncmp(cmd, "AT+CMGS=", 8) && ++cmgs == fail_cmgs) return HAL_AT_ERROR;
    snprintf(resp, cap, "%s", !strncmp(cmd, "AT+CMGR=", 8) ? reply : "");
    return HAL_AT_OK;
}
hal_at_result_t hal_modem_at(const char *cmd, char *resp, size_t cap, uint32_t timeout_ms) { return hal_modem_at_payload(cmd, NULL, resp, cap, timeout_ms); }
uint64_t osal_now_us(void) { return now_us; }
void osal_task_delay_us(uint64_t us) { now_us += us; }

static void reset(int fail) { ncmd = cmgs = 0; fail_cmgs = fail; }

static void han(char *out, int n) { for (int i = 0; i < n; i++) memcpy(out + 3 * i, "中", 4); }

int main(void) {
    static char text[1200], number[24], got[400];
    int ends[10], sent = -1, concat[3];
    bool gsm7;

    /* how a text is split */
    EXPECT(nt26_sms_split("hellohello", ends, 10, &gsm7) == 1 && gsm7 && ends[0] == 10, "short GSM text is one part");
    memset(text, 'a', 160); text[160] = 0;
    EXPECT(nt26_sms_split(text, ends, 10, &gsm7) == 1 && gsm7, "160 GSM characters are one part");
    text[160] = 'a'; text[161] = 0;
    EXPECT(nt26_sms_split(text, ends, 10, &gsm7) == 2 && ends[0] == 153 && ends[1] == 161, "161 go as 153 + 8");
    memset(text, 0, sizeof text); han(text, 70);
    EXPECT(nt26_sms_split(text, ends, 10, &gsm7) == 1 && !gsm7, "70 Chinese characters are one part");
    han(text, 150);
    EXPECT(nt26_sms_split(text, ends, 10, &gsm7) == 3 && ends[0] == 201 && ends[1] == 402 && ends[2] == 450, "150 go as 67 + 67 + 16");
    memset(text, 0, sizeof text); han(text, 66); strcat(text, "\xF0\x9F\x98\x80"); han(text + strlen(text), 10);
    EXPECT(nt26_sms_split(text, ends, 10, &gsm7) == 2 && ends[0] == 198, "a surrogate pair is never split (%d)", ends[0]);
    EXPECT(nt26_sms_split("a[b", ends, 10, &gsm7) == 1 && !gsm7, "extension characters go as UCS2");
    EXPECT(nt26_sms_split("\xC0\x80", ends, 10, &gsm7) == -1, "malformed UTF-8 is refused");

    /* SMS-SUBMIT */
    reset(0);
    EXPECT(nt26_sms_send_parts("+46708251358", "hellohello", 30000, &sent) == NT26_OK && sent == 1, "a short message goes");
    EXPECT(ncmd == 2 && !strcmp(cmds[0], "AT+CMGF=0") && !strcmp(cmds[1], "AT+CMGS=22") &&
           !strcmp(payloads[1], "0001000B916407281553F800000AE8329BFD4697D9EC37"), "the classic SUBMIT (%s %s)", cmds[1], payloads[1]);
    memset(text, 0, sizeof text); han(text, 150);
    reset(0);
    EXPECT(nt26_sms_send_parts("10086", text, 30000, &sent) == NT26_OK && sent == 3 && ncmd == 4, "150 Chinese characters go in 3 parts");
    char ref[3];
    snprintf(ref, sizeof ref, "%.2s", payloads[1] + 28);
    for (int p = 1; p <= 3; p++) {
        char head[64];
        snprintf(head, sizeof head, "00410005810180F60008%s050003%s03%02d", p < 3 ? "8C" : "26", ref, p);
        EXPECT(!strncmp(payloads[p], head, strlen(head)) && !strncmp(payloads[p] + strlen(head), "4E2D4E2D", 8),
               "part %d: SUBMIT with UDHI, UCS2, header 05 00 03 (%.40s)", p, payloads[p]);
        EXPECT(!strcmp(cmds[p], p < 3 ? "AT+CMGS=150" : "AT+CMGS=48"), "part %d length (%s)", p, cmds[p]);
    }
    memset(text, 'A', 200); text[200] = 0;
    reset(0);
    EXPECT(nt26_sms_send_parts("10086", text, 30000, &sent) == NT26_OK && sent == 2, "200 GSM characters go in 2 parts");
    /* 7 header septets (6 octets and a fill bit), then 153 'A' (0x41): the first data septet starts at bit 49 */
    EXPECT(!strncmp(payloads[1] + 20, "A0", 2) && !strncmp(payloads[1] + 22, "050003", 6) && !strncmp(payloads[1] + 34, "82C1", 4),
           "GSM part 1: UDL 160, header, 'A' after the fill bit (%.40s)", payloads[1]);
    EXPECT(!strncmp(payloads[2] + 20, "36", 2), "GSM part 2: UDL 7 + 47 = 54 (%.24s)", payloads[2]);

    /* a part fails: what went is counted, nothing after it is tried */
    han(text, 150); text[450] = 0;
    reset(2);
    EXPECT(nt26_sms_send_parts("10086", text, 30000, &sent) == NT26_AT_FAILED && sent == 1 && ncmd == 3, "part 2 refused: 1 went (%d)", sent);
    reset(0);
    EXPECT(nt26_sms_send_parts("10086", text, 4000, &sent) == NT26_NOT_SENT && sent == 1, "too little time for part 2: not tried (%d)", sent);

    /* SMS-DELIVER */
    snprintf(reply, sizeof reply, "+CMGR: 0,,30\n07911326040000F0040B911346610089F60000208062917314080CC8F71D14969741F977FD07");
    EXPECT(nt26_sms_read_part(3, number, sizeof number, got, sizeof got, concat) == NT26_OK && !strcmp(number, "+31641600986") &&
           !strcmp(got, "How are you?") && concat[1] == 1, "the classic DELIVER (%s: %s)", number, got);
    /* UCS2, 16-bit reference 0x1234, part 2 of 3, from 10086: "你好" */
    snprintf(reply, sizeof reply, "+CMGR: 0,,27\n0891683108000000F04405810180F60008621102010000230B060804123403024F60597D");
    EXPECT(nt26_sms_read_part(1, number, sizeof number, got, sizeof got, concat) == NT26_OK && !strcmp(number, "10086") &&
           !strcmp(got, "你好") && concat[0] == 0x1234 && concat[1] == 3 && concat[2] == 2, "UCS2 part (%s %d/%d/%d)", got, concat[0], concat[1], concat[2]);
    /* GSM 7-bit, 8-bit reference 0x2A, part 1 of 2: "hi" after header and fill bit; an extension character too: "€" */
    snprintf(reply, sizeof reply, "+CMGR: 0,,26\n004405810180F60000621102010000230B0500032A0201D0E94D19");
    EXPECT(nt26_sms_read_part(1, number, sizeof number, got, sizeof got, concat) == NT26_OK && !strcmp(got, "hi€") &&
           concat[0] == 0x2A && concat[1] == 2 && concat[2] == 1, "GSM part (%s %d/%d/%d)", got, concat[0], concat[1], concat[2]);
    /* a part number of 0 is ignored with its header (§9.2.3.24.1): a whole message */
    snprintf(reply, sizeof reply, "+CMGR: 0,,27\n0891683108000000F04405810180F60008621102010000230B060804123403004F60597D");
    EXPECT(nt26_sms_read_part(1, number, sizeof number, got, sizeof got, concat) == NT26_OK && concat[1] == 1 && concat[2] == 1, "bad part number");
    snprintf(reply, sizeof reply, "+CMGR: 0,,14\n0891683108000000F00405810180F600046211020100002302ABCD");
    EXPECT(nt26_sms_read_part(1, number, sizeof number, got, sizeof got, concat) == NT26_AT_FAILED, "8-bit data is not read as text");
    printf("%s\n", failures ? "FAILED" : "ok");
    return failures != 0;
}
