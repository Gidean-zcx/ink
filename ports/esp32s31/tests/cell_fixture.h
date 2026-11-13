#ifndef INK_CELL_FIXTURE_H
#define INK_CELL_FIXTURE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { F_SPLIT, F_NO_WAKE, F_NO_ACK, F_NO_RESPONSE, F_SILENT_GREETING, F_STARTED, F_BAUD, F_DROP_PROBES,
       F_MISSING, F_UNREGISTERED, F_CSQ, F_BAD_NAT, F_INTERLEAVE, F_DROP_RX, F_NO_DHCP, F_NO_PDU_FINAL, F_CORRUPT,
       F_CALL /* 1 ATD answers BUSY, 2 NO ANSWER; 3 AT+CHUP answers NO CARRIER only; AT+CLCC lists 4 alerting, 5 active */ };
enum { N_WRITES, N_HANDSHAKES, N_RADIO, N_LOANS, N_RETURNS, N_HELD, N_STARTS, N_STOPS, N_RX, N_TX,
       N_BAD_HEADERS, N_CHANGED, N_BAUD, N_SLOT, N_FORBIDDEN, N_WAKE_LEVEL, N_TASKS, N_PROBES, N_DROPPED_PROBES, N_ACKS, N_DNS };
void ink_test_set(int, int);
unsigned ink_test_count(int);
void ink_test_reply(const char *);
void ink_test_ethernet(void);
bool ink_test_transmit(void);
bool ink_test_gap_during_at(void);
void ink_test_wait_count(int, unsigned);
void ink_test_rebuild_status(void);
void ink_test_ip(bool);
void ink_test_advance(unsigned);
void ink_test_drain(void);
void ink_test_finish(void);
const char *ink_test_logs(void);
void ink_test_wait_log(const char *);
bool ink_test_bh(void);
#ifdef __cplusplus
}
#endif
#endif
