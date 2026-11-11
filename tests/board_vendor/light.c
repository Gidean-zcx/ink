/* Drive the real light service with a deterministic shared-timer fake. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/hal_power.h"
#include "ink/sys/display.h"
#include "ink/sys/settings.h"

#define CHECK(ok, why) do { if (!(ok)) { fprintf(stderr, "%s (line %d)\n", why, __LINE__); exit(1); } checks++; } while (0)
struct osal_mutex { bool held; };
struct osal_timer { osal_timer_fn fn; void *arg; bool active; uint64_t next, period; };
static struct osal_mutex lock;
static struct osal_timer timer;
static const ink_setting_def_t *settings;
static ink_state_fn power_changed;
static const char *power_state = "active";
static int64_t brightness = 10, warmth = 50;
static int checks, cold, warm;
static uint64_t now;

osal_mutex_t *osal_mutex_create(void) { return &lock; }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t timeout) {
    (void)timeout;
    CHECK(m == &lock && !m->held, "light mutex serializes state");
    m->held = true;
    return OSAL_OK;
}
void osal_mutex_unlock(osal_mutex_t *m) { CHECK(m->held, "unlock held mutex"); m->held = false; }
osal_timer_t *osal_timer_create(const char *name, osal_timer_fn fn, void *arg) {
    CHECK(!strcmp(name, "frontlight"), "uses shared timer");
    timer.fn = fn; timer.arg = arg;
    return &timer;
}
osal_err_t osal_timer_start(osal_timer_t *t, uint64_t delay, uint64_t period) {
    CHECK(t == &timer && delay == 5000 && period == 5000, "frontlight period is 5 ms");
    t->active = true; t->next = now + delay; t->period = period;
    return OSAL_OK;
}
osal_err_t osal_timer_stop(osal_timer_t *t) { t->active = false; return OSAL_OK; }
void hal_light_set(int c, int w) { cold = c; warm = w; }
int ink_settings_define(const ink_setting_def_t *defs, int n) { CHECK(n == 2, "existing settings unchanged"); settings = defs; return 0; }
int64_t ink_setting_get(const char *id) { return !strcmp(id, "display.light") ? brightness : warmth; }
int ink_state_watch(const char *prefix, ink_state_fn fn, void *ctx, ink_exec_t exec) {
    (void)ctx; (void)exec;
    CHECK(!strcmp(prefix, "power.state"), "watches power state");
    power_changed = fn;
    return 0;
}
int ink_state_get_str(int h, char *out, size_t cap) { (void)h; return snprintf(out, cap, "%s", power_state); }
static void advance(uint64_t us) {
    uint64_t end = now + us;
    while (timer.active && timer.next <= end) {
        now = timer.next; timer.next += timer.period; timer.fn(timer.arg);
    }
    now = end;
}
static void set(int b) { brightness = b; settings[0].apply(NULL, b); }
int main(void) {
    CHECK(ink_light_init() == 0 && cold + warm == 100 && !timer.active, "saved brightness restores immediately");
    set(13);
    CHECK(cold + warm == 100, "setting does not jump straight to target");
    advance(4999);
    CHECK(cold + warm == 100, "no change before 5 ms");
    advance(1);
    CHECK(cold + warm == 110, "exactly one level at 5 ms");
    advance(5000);
    CHECK(cold + warm == 120, "second level at 10 ms");
    advance(5000);
    CHECK(cold + warm == 130 && !timer.active, "target reached; timer stops");
    set(10); advance(5000);
    CHECK(cold + warm == 120, "decrease also moves one level");
    set(14); advance(10000);
    CHECK(cold + warm == 140 && !timer.active, "new target reverses active transition");
    warmth = 100; settings[1].apply(NULL, warmth);
    CHECK(cold == 0 && warm == 140, "warmth uses current brightness");
    set(80); advance(5000); ink_light_off();
    CHECK(cold == 0 && warm == 0 && !timer.active, "screen off forces zero immediately");
    advance(100000); set(90); advance(100000);
    CHECK(cold == 0 && warm == 0 && !timer.active, "screen off cancels pending fade and suppresses settings");
    power_changed(NULL, 0, "power.state");
    CHECK(cold + warm == 900 && !timer.active, "wake restores saved brightness immediately as vendor");
    set(10); advance(5000); power_state = "locked"; power_changed(NULL, 0, "power.state");
    CHECK(cold == 0 && warm == 0 && !timer.active, "lock cancels fade immediately");
    printf("board-vendor light: %d checks passed\n", checks);
    return 0;
}
