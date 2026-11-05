/* Original service unchanged; inspect only a synthetic secret's lifetime. */
#include "../../../../system/policy/confirm.c"
int audit_retired_confirm_has(const char *marker) {
    int found = 0;
    char value[64];
    ink_svc_lock();
    for (int i = 0; i < MAX_HELD; i++)
        if (!CF.held[i].used && ink_args_get_str(&CF.held[i].args, "password", value, sizeof value) >= 0 && !strcmp(value, marker)) found++;
    ink_svc_unlock();
    return found;
}
