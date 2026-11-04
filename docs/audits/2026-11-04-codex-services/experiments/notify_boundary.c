/* Product source is included unchanged. Only the unlock boundary is observable. */
#include "ink/sys/svc.h"
extern void audit_notify_unlock(void);
#define ink_svc_unlock audit_notify_unlock
#include "../../../../system/services/notify/notify_service.c"
#undef ink_svc_unlock
void audit_notify_member(char *out, size_t cap) {
    ink_svc_lock();
    snprintf(out, cap, "%s", NS.member);
    ink_svc_unlock();
}
