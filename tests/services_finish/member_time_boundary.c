/* Pause before the real edit lock or event delivery, without changing their results. */
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/cap.h"
extern volatile bool ink_test_armed, ink_test_waiting, ink_test_release;
extern volatile bool ink_test_event_armed, ink_test_event_waiting, ink_test_event_release;
static osal_mutex_t *ink_test_edits;
static osal_err_t ink_test_lock(osal_mutex_t *m, int64_t timeout) {
    if (ink_test_armed && m == ink_test_edits) {
        ink_test_armed = false;
        ink_test_waiting = true;
        while (!ink_test_release) osal_task_delay_us(10000);
    }
    return osal_mutex_lock(m, timeout);
}
static void ink_test_before_time_event(const char *id, const ink_args_t *args) {
    if (!ink_test_event_armed || (strcmp(id, "alarm.missed") && strcmp(id, "reminder.missed")) || !args) return;
    char title[48] = "";
    ink_args_get_str(args, "title", title, sizeof title);
    if (!strstr(title, "SYNTH_PRIVATE")) return;
    ink_test_event_armed = false;
    ink_test_event_waiting = true;
    while (!ink_test_event_release) osal_task_delay_us(10000);
}
static void ink_test_emit(const char *id, const ink_args_t *args) {
    ink_test_before_time_event(id, args);
    ink_cap_emit(id, args);
}
static void ink_test_emit_for(const char *id, const ink_args_t *args, const char *member, uint32_t generation) {
    ink_test_before_time_event(id, args);
    ink_cap_emit_for(id, args, member, generation);
}
#define osal_mutex_lock ink_test_lock
#define ink_cap_emit ink_test_emit
#define ink_cap_emit_for ink_test_emit_for
#ifndef INK_TEST_TIME_SOURCE
#define INK_TEST_TIME_SOURCE "../../system/services/time/time_service.c"
#endif
#include INK_TEST_TIME_SOURCE
#undef osal_mutex_lock
#undef ink_cap_emit
#undef ink_cap_emit_for
void ink_test_hold(void) { ink_test_edits = T.edits; }
void ink_test_unlock(void) { }
/* Drive the real due handler from a worker so storage timers remain able to run. */
void ink_test_due_after(int64_t utc_s) {
    osal_set_wall_us(utc_s * 1000000);
    due_cb(NULL);
}
