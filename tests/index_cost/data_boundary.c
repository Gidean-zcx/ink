#include "measure.h"
#include "ink/sys/data.h"
#define ink_db_wait cost_actual_wait
#include "../../system/services/data/datastore.c"
#undef ink_db_wait
int ink_db_wait(ink_db_t *db, uint32_t ver, int64_t timeout_us) {
    uint64_t begin = osal_now_us();
    int result = cost_actual_wait(db, ver, timeout_us);
    ink_cost_wait(db ? db->def->name : "null", begin, osal_now_us(), result);
    return result;
}
