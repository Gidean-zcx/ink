/*
 * The notification service unchanged, with one thing added: a moment, just after it let go of the lock, where a test can
 * have something else happen (tests/integration/races.c).
 */
#include "ink/sys/svc.h"

extern void races_notify_unlock(void);
#define ink_svc_unlock races_notify_unlock
#include "../../system/services/notify/notify_service.c"
#undef ink_svc_unlock

void races_notify_member(char *out, size_t cap) {
    ink_svc_lock();
    snprintf(out, cap, "%s", NS.member);
    ink_svc_unlock();
}
