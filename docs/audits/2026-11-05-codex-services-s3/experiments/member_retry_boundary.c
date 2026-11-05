/* Pause before a real datastore read during a retry. Product statements stay unchanged. */
#include "ink/sys/data.h"
#include "ink/osal.h"
static int audit_next(ink_db_t *, ink_db_cursor_t *, ink_rec_t *, void *, size_t);
#define ink_db_next audit_next
#include "../../../../system/policy/members.c"
#undef ink_db_next
static osal_sem_t *go;
static volatile int armed, entered;
void audit_arm_retry(void) { go = osal_sem_create(0, 1); armed = 1; }
int audit_retry_entered(void) { return entered; }
void audit_release_retry(void) { osal_sem_give(go); }
static int audit_next(ink_db_t *db, ink_db_cursor_t *c, ink_rec_t *r, void *p, size_t n) {
    if (armed) {
        armed = 0; entered = 1;
        osal_sem_take(go, OSAL_WAIT_FOREVER);
    }
    return ink_db_next(db, c, r, p, n);
}
