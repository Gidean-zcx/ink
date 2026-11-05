/* Hold a real producer after persistence and before the bus takes its payload. */
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/cap.h"
extern volatile bool ink_test_event_armed, ink_test_event_waiting, ink_test_event_release;
static void ink_test_before_notify_event(const char *id, const ink_args_t *args) {
    if (!ink_test_event_armed || strcmp(id, "notify.posted") || !args) return;
    char title[48] = "";
    ink_args_get_str(args, "title", title, sizeof title);
    if (!strstr(title, "SYNTH_PRIVATE")) return;
    ink_test_event_armed = false;
    ink_test_event_waiting = true;
    while (!ink_test_event_release) osal_task_delay_us(10000);
}
static void ink_test_emit(const char *id, const ink_args_t *args) {
    ink_test_before_notify_event(id, args);
    ink_cap_emit(id, args);
}
static void ink_test_emit_for(const char *id, const ink_args_t *args, const char *member, uint32_t generation) {
    ink_test_before_notify_event(id, args);
    ink_cap_emit_for(id, args, member, generation);
}
#define ink_cap_emit ink_test_emit
#define ink_cap_emit_for ink_test_emit_for
#ifndef INK_TEST_NOTIFY_SOURCE
#define INK_TEST_NOTIFY_SOURCE "../../system/services/notify/notify_service.c"
#endif
#include INK_TEST_NOTIFY_SOURCE
#undef ink_cap_emit
#undef ink_cap_emit_for
