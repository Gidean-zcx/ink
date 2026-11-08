#include "ink/osal.h"
#include "ink/hal_power.h"
#include "ink/sys/data.h"
#include "ink/probe.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void cut_power(void) {
    ink_probe_count("test.cut");
    hal_power_reboot();
    for (;;) osal_task_delay_us(1000000);
}

static int cut_put(ink_db_t *db, const ink_db_write_t *w, uint32_t *id) {
    const char *cut = getenv("INK_CONFIRM_CUT");
    const char *state = strstr(w->data, "\"state\":\"running\"") ? "running" :
        strstr(w->data, "\"state\":\"done\"") ? "done" : "other";
    char before[40], after[40];
    snprintf(before, sizeof before, "before-%s", state);
    snprintf(after, sizeof after, "after-%s", state);
    if (cut && !strcmp(cut, before)) cut_power();
    int result = ink_db_put(db, w, id);
    if (!result && cut && !strcmp(cut, after)) {
        if (ink_db_wait(db, *w->written_ver, 5000000)) return INK_DB_ERR_FULL;
        cut_power();
    }
    return result;
}

static int result_wait(ink_db_t *db, uint32_t ver, int64_t timeout) {
    const char *phase = getenv("INK_CONFIRM_PHASE");
    if (phase && !strcmp(phase, "failure")) {
        ink_db_cursor_t at = {0}; ink_rec_t rec; char out[2048];
        while (ink_db_next(db, &at, &rec, out, sizeof out) == 1)
            if (rec.ver == ver && strstr(out, "\"state\":\"running\"")) return -1;
    }
    return ink_db_wait(db, ver, timeout);
}
#define ink_db_wait result_wait
#define ink_db_put cut_put
#ifndef INK_TEST_CONFIRM_SOURCE
#define INK_TEST_CONFIRM_SOURCE "../../system/policy/confirm.c"
#endif
#include INK_TEST_CONFIRM_SOURCE
#undef ink_db_put
#undef ink_db_wait

void review_request(const ink_json_t *j) { on_request(NULL, 100, "confirm/request", j, 0); }
unsigned review_count(void) { return (unsigned)ink_db_count(CF.db); }
unsigned review_last_token(void) { return CF.next_token; }
bool review_running_durable(uint32_t token) {
    if (!token) token = CF.next_token;
    ink_rec_t rec;
    char *out = osal_calloc(1, 2048, OSAL_MEM_EXTERNAL);
    bool durable = out && ink_db_get(CF.db, token, &rec, out, 2047) == 0 &&
        rec.ver <= ink_db_durable_ver(CF.db) && strstr(out, "\"state\":\"running\"");
    osal_free(out);
    return durable;
}
