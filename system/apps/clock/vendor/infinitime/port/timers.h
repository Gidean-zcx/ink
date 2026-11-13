/* clock-3 shim for InfiniTime's <timers.h> (SOURCE.md): a one-shot FreeRTOS software timer is a service deadline
 * (ink_svc_at). Handles come from a fixed pool (8 members x 6 timers). The callback runs in the service timer task with
 * the handle already marked stopped, as FreeRTOS does; `active` and `expiry` are read on the UI task and written there
 * and in the timer task, so they are kept under the shared lock, and ink_svc_at/ink_svc_cancel are called outside it.
 * A handle also counts as stopped once its expiry tick has passed, before the timer task has run the callback, so a
 * remaining time is never computed past zero. Everything is static: nothing here is linked by name. */
#ifndef TIMERS_H
#define TIMERS_H
#include "FreeRTOS.h"
#include "ink/sys/svc.h"
#ifndef INK_PSRAM_BSS
#define INK_PSRAM_BSS
#endif
enum { INK_INFINITIME_TIMERS = 48 };
struct ink_infinitime_timer;
typedef ink_infinitime_timer *TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t);
struct ink_infinitime_timer {
    ink_svc_deadline_t due;
    TimerCallbackFunction_t callback;
    void *id;
    TickType_t period, expiry;
    bool active;
};
static ink_infinitime_timer ink_infinitime_pool[INK_INFINITIME_TIMERS] INK_PSRAM_BSS;
static int ink_infinitime_used;
static inline void ink_infinitime_due(void *arg) {
    ink_infinitime_timer *t = static_cast<ink_infinitime_timer *>(arg);
    ink_svc_lock();
    bool was = t->active;
    t->active = false;
    ink_svc_unlock();
    if (was) t->callback(t);
}
static inline TimerHandle_t xTimerCreate(const char *, TickType_t period, UBaseType_t, void *id, TimerCallbackFunction_t callback) {
    if (ink_infinitime_used >= INK_INFINITIME_TIMERS) return nullptr;
    ink_infinitime_timer *t = &ink_infinitime_pool[ink_infinitime_used++];
    t->callback = callback; t->id = id; t->period = period;
    return t;
}
static inline BaseType_t xTimerChangePeriod(TimerHandle_t t, TickType_t period, TickType_t) { t->period = period; return pdTRUE; }
static inline BaseType_t xTimerStart(TimerHandle_t t, TickType_t) {
    uint64_t now = INK_INFINITIME_NOW_US() / 1000;
    ink_svc_lock();
    t->expiry = (TickType_t)now + t->period; t->active = true;
    ink_svc_unlock();
    ink_svc_at(&t->due, (now + t->period) * 1000, ink_infinitime_due, t);
    return pdTRUE;
}
static inline BaseType_t xTimerStop(TimerHandle_t t, TickType_t) {
    ink_svc_lock();
    t->active = false;
    ink_svc_unlock();
    ink_svc_cancel(&t->due);
    return pdTRUE;
}
static inline TickType_t xTimerGetExpiryTime(TimerHandle_t t) {
    ink_svc_lock();
    TickType_t expiry = t->expiry;
    ink_svc_unlock();
    return expiry;
}
static inline BaseType_t xTimerIsTimerActive(TimerHandle_t t) {
    TickType_t now = xTaskGetTickCount();
    ink_svc_lock();
    bool active = t->active && (int32_t)(t->expiry - now) > 0;
    ink_svc_unlock();
    return active ? pdTRUE : pdFALSE;
}
static inline void *pvTimerGetTimerID(TimerHandle_t t) { return t->id; }
#endif
