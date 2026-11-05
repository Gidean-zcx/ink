/* Device-side enrollment. Credentials never enter the public snapshot. */
#ifndef INK_SYS_PAIRING_H
#define INK_SYS_PAIRING_H
#include <stdbool.h>
#include <stddef.h>
#include "ink/sys/json.h"

typedef enum { INK_PAIR_UNPAIRED, INK_PAIR_PAIRING, INK_PAIR_COMMITTING, INK_PAIR_BOUND, INK_PAIR_REVOKED } ink_pair_state_t;
int ink_pairing_init(void);
void ink_pairing_start(void);
uint64_t ink_pairing_deadline(void);
bool ink_pairing_enabled(void);
bool ink_pairing_blocked(void);
bool ink_pairing_wants_link(void);
/* Hub task only: copy the current endpoint and trust anchor. */
bool ink_pairing_endpoint(char url[128], unsigned char pin[32], bool *pinned);
int ink_pairing_handshake(char method[32], char *params, size_t cap);
/* NULL clears connection trust before opening the next socket. */
void ink_pairing_peer(const unsigned char hash[32]);
void ink_pairing_reply(bool hello, int status, const ink_json_t *j, int result);
/* True when consumed; only enrollment requests are accepted before authentication. */
bool ink_pairing_request(long long id, const char *method, const ink_json_t *j, int params);
void ink_pairing_expired(void);
ink_pair_state_t ink_pairing_state(void);
#endif
