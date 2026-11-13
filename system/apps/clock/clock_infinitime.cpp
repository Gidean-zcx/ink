/* clock-3: the copied InfiniTime controllers behind a C interface (vendor/infinitime/SOURCE.md). The clock's one C++
 * unit: it includes the copied files once, after our shims. Nothing is built before it is first used (no global
 * constructors); the storage is static PSRAM. UI task, except the two deadline callbacks, which only post to it. */
#include <new>
#include <stdint.h>
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/exec.h"
static uint64_t ck_now_us() { return osal_now_us(); }
#define INK_INFINITIME_NOW_US ck_now_us
#include "FreeRTOS.h"
#include "timers.h"
#ifndef INK_INFINITIME_SHIM
#error "InfiniTime's <FreeRTOS.h> and <timers.h> must be the shims in vendor/infinitime/port, never the SDK's"
#endif
#include "components/stopwatch/StopWatchController.cpp"
#include "components/timer/Timer.cpp"
#include "displayapp/screens/Timer.h"
/* Inside their own namespaces: the copied files' `using namespace` lines would make `Timer` ambiguous here. */
namespace Pinetime::Applications::Screens {
#include "displayapp/screens/timer_screen_part.cpp"
}
namespace Pinetime::Applications::Widgets {
#include "displayapp/widgets/counter_part.cpp"
}
#include "clock_infinitime.h"

namespace PS = Pinetime::Applications::Screens;
namespace PC = Pinetime::Controllers;
namespace PW = Pinetime::Applications::Widgets;
namespace {
ck_hooks_t hooks;
void expired(TimerHandle_t handle);
struct Unit {
    PC::Timer timer;
    PC::MotorController motor;
    PS::Timer screen;
    ink_svc_deadline_t quiet;
    ink_exec_job_t due, hush;
    uint32_t duration = 0;
    uint8_t state = CK_IDLE;
    bool changed = false; /* its counters were written: the row's digits change */
    explicit Unit(void *self) : timer(self, expired), screen(timer, motor), quiet(), due(), hush() {
        for (PW::Counter *c : {&screen.hourCounter, &screen.minuteCounter, &screen.secondCounter}) c->userData = &changed;
    }
};
struct Edit {
    PW::Counter part[3] = {PW::Counter(0, 23), PW::Counter(0, 59), PW::Counter(0, 59)};
};
alignas(Unit) unsigned char units[CK_TIMERS][sizeof(Unit)] INK_PSRAM_BSS;
alignas(PC::StopWatchController) unsigned char watches[CK_WATCHES][sizeof(PC::StopWatchController)] INK_PSRAM_BSS;
alignas(Edit) unsigned char edit[sizeof(Edit)] INK_PSRAM_BSS;
bool built[CK_TIMERS], watch_built[CK_WATCHES], edit_built;

Unit &U(int t) {
    if (!built[t]) { new (units[t]) Unit(units[t]); built[t] = true; }
    return *std::launder(reinterpret_cast<Unit *>(units[t]));
}
int index_of(const Unit &u) { return (int)((reinterpret_cast<const unsigned char *>(&u) - units[0]) / (long)sizeof(Unit)); }
Unit &owner(const PS::Timer *screen) {
    int t = 0;
    while (t < CK_TIMERS - 1 && !(built[t] && &U(t).screen == screen)) t++;
    return U(t);
}
PC::StopWatchController &W(int w) {
    if (!watch_built[w]) { new (watches[w]) PC::StopWatchController(); watch_built[w] = true; }
    return *std::launder(reinterpret_cast<PC::StopWatchController *>(watches[w]));
}
Edit &E() {
    if (!edit_built) {
        new (edit) Edit();
        for (int i = 0; i < 3; i++) std::launder(reinterpret_cast<Edit *>(edit))->part[i].userData = reinterpret_cast<void *>((uintptr_t)i + 1);
        edit_built = true;
    }
    return *std::launder(reinterpret_cast<Edit *>(edit));
}
/* Glue the copied functions cannot do: what the row shows after zero is the glue's, and the sound stops sixty
 * seconds after zero by a deadline of its own, whatever page is up (SOURCE.md, "接线层补的"). */
void quiet_ui(void *arg) {
    Unit &u = *static_cast<Unit *>(arg);
    if (u.state == CK_FINISHED) u.screen.Refresh(); /* copied: past 60 s it stops the ringing and lets the wake lock go */
}
void quiet_due(void *arg) { ink_exec_post(INK_EXEC_UI, &static_cast<Unit *>(arg)->hush, quiet_ui, arg); }
void expired_ui(void *arg) {
    Unit &u = *static_cast<Unit *>(arg);
    if (u.state != CK_RUNNING) return; /* stopped or reset meanwhile */
    u.state = CK_FINISHED;
    u.screen.SetTimerRinging();
    if (hooks.finished) hooks.finished(index_of(u));
}
void expired(TimerHandle_t handle) { /* the service timer task: only post */
    Unit *u = static_cast<Unit *>(pvTimerGetTimerID(handle));
    ink_exec_post(INK_EXEC_UI, &u->due, expired_ui, u);
}
void to_idle(Unit &u, uint32_t seconds) {
    if (u.state == CK_RUNNING) u.screen.ToggleRunning(); /* copied: stop */
    if (u.state == CK_FINISHED) u.screen.ToggleRunning(); /* copied: StopRinging and Reset */
    else u.screen.Reset();                                 /* copied */
    ink_svc_cancel(&u.quiet);
    if (u.motor.card > 0) ink_interrupt_end(u.motor.card);
    u.motor.card = 0;
    u.duration = seconds; u.state = CK_IDLE;
    u.screen.hourCounter.SetValue((int)(seconds / 3600)); /* Fossify's reset: back to the set duration */
    u.screen.minuteCounter.SetValue((int)(seconds / 60 % 60));
    u.screen.secondCounter.SetValue((int)(seconds % 60));
}
}

void PS::Timer::SetTimerRunning() { wakeLock.Lock(); }
void PS::Timer::SetTimerStopped() { wakeLock.Release(); }
void PS::Timer::SetTimerRinging() {
    Unit &u = owner(this);
    motorController.StartRinging();
    wakeLock.Lock();
    motorController.card = hooks.ring ? hooks.ring(index_of(u)) : -1;
    motorController.ringing = motorController.card > 0;
    if (!motorController.ringing) { /* the card table is full: the row says it, nothing rings */
        wakeLock.Release();
        ink_probe_count("clock.timer_card_full");
        return;
    }
    auto status = timer.GetTimerState();
    int64_t since = status ? status->distanceToExpiry.count() : 0;
    ink_svc_at(&u.quiet, osal_now_us() + (uint64_t)(since < 60001 ? 60001 - since : 0) * 1000, quiet_due, &u);
}
void PW::Counter::UpdateLabel() {
    uintptr_t tag = reinterpret_cast<uintptr_t>(userData);
    if (tag >= 1 && tag <= 3) { if (hooks.edit_label) hooks.edit_label((int)tag - 1); }
    else if (userData) *static_cast<bool *>(userData) = true;
}

extern "C" {
void ck_init(const ck_hooks_t *h) { hooks = *h; }
void ck_watch_start(int w) { if (!W(w).IsRunning()) W(w).Start(); }
void ck_watch_pause(int w) { if (W(w).IsRunning()) W(w).Pause(); }
void ck_watch_clear(int w) { W(w).Clear(); }
bool ck_watch_lap(int w) {
    if (!W(w).IsRunning() || W(w).GetMaxLapNumber() >= CK_LAPS) return false; /* the copied buffer never wraps */
    W(w).AddLapToHistory();
    return true;
}
int ck_watch_state(int w) { return W(w).IsRunning() ? 1 : W(w).IsPaused() ? 2 : 0; }
uint32_t ck_watch_ms(int w) { return W(w).GetElapsedTime(); }
int ck_watch_laps(int w) { return W(w).GetMaxLapNumber(); }
uint32_t ck_watch_lap_ms(int w, int number) {
    auto lap = W(w).GetLapFromHistory((uint8_t)(W(w).GetMaxLapNumber() - number));
    return lap ? lap->timeSinceStart : 0;
}
void ck_edit_set(int part, int value) { E().part[part].SetValue(value); }
int ck_edit_get(int part) { return E().part[part].GetValue(); }
void ck_edit_step(int part, int delta) {
    if (delta > 0) E().part[part].UpBtnPressed();
    else E().part[part].DownBtnPressed();
}
void ck_timer_set(int t, uint32_t seconds) { to_idle(U(t), seconds); }
int ck_timer_state(int t) { return U(t).state; }
uint32_t ck_timer_duration(int t) { return U(t).duration; }
uint32_t ck_timer_shown(int t) {
    PS::Timer &s = U(t).screen;
    return U(t).state == CK_FINISHED ? 0 : (uint32_t)(s.hourCounter.GetValue() * 3600 + s.minuteCounter.GetValue() * 60 + s.secondCounter.GetValue());
}
uint32_t ck_timer_left_ms(int t) {
    if (U(t).state != CK_RUNNING) return ck_timer_shown(t) * 1000;
    auto status = U(t).timer.GetTimerState();
    return status && !status->expired ? (uint32_t)status->distanceToExpiry.count() : 0;
}
bool ck_timer_tick(int t) {
    Unit &u = U(t);
    if (u.state != CK_RUNNING || !u.timer.IsRunning()) return false;
    u.changed = false;
    u.screen.Refresh(); /* copied: DisplayTime writes the counters when the second changed (DirtyValue) */
    return u.changed;
}
void ck_timer_toggle(int t) {
    Unit &u = U(t);
    auto status = u.timer.GetTimerState();
    if (u.state == CK_FINISHED || (status && status->expired)) return;
    bool was = u.state == CK_RUNNING;
    u.screen.ToggleRunning(); /* copied */
    u.state = u.timer.IsRunning() ? (uint8_t)CK_RUNNING : was ? (uint8_t)CK_PAUSED : u.state;
}
void ck_timer_restart(int t) {
    Unit &u = U(t);
    to_idle(u, u.duration);
    u.screen.ToggleRunning(); /* copied: starts from the counters */
    u.state = u.timer.IsRunning() ? CK_RUNNING : CK_IDLE;
}
}
