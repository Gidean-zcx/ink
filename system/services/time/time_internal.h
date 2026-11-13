/* Inside the time service. */
#ifndef INK_TIME_INTERNAL_H
#define INK_TIME_INTERNAL_H

#include <stdint.h>
#include "ink/sys/args.h"
#include "ink/sys/time.h"

int ink_time_caps_init(void);
int64_t time_days_from_civil(int y, int m, int d);
void time_civil_from_days(int64_t days, int *y, int *m, int *d);
/* Internal callers keep the identity accepted by the capability bus. */
int ink_time_alarm_add_for(ink_alarm_t *a, uint32_t *written, const char *member);
int ink_time_alarm_update_for(ink_alarm_t *a, const char *member);
int ink_time_alarm_cancel_for(uint32_t id, uint32_t revision, const char *member);
int ink_time_alarm_next_for(int *cursor, ink_alarm_kind_t kind, ink_alarm_t *out, const char *member);

/* events.c: the calendar's events (calendar-3). */
void time_reschedule(void);
void time_edits_lock(void);
void time_edits_unlock(void);
void time_emit_for(const char *event, const ink_args_t *args, const char *member);
uint32_t time_new_id(void);
int time_events_open(void);
bool time_events_adopt(const ink_alarm_t *legacy); /* a reminder from time.alarms is in time.events now */
uint32_t time_events_max_id(void);
void time_events_replan(int64_t now, bool all);
int64_t time_events_earliest(void);
void time_events_due(int64_t now);
int time_events_forget(const char *member);
bool time_events_occurrence(uint32_t id, int64_t from, int64_t *out);
int time_events_project(int *cursor, ink_alarm_t *out, const char *member);
int time_events_from_alarm(const ink_alarm_t *a, ink_event_t *e);
int ink_time_event_add_for(ink_event_t *e, uint32_t *written, const char *member);
/* occurrence: the original local start (UTC) of the occurrence meant, for THIS and FOLLOWING. */
int ink_time_event_update_for(ink_event_t *e, int64_t occurrence, ink_scope_t scope, const char *member, uint32_t *new_id);
int ink_time_event_delete_for(uint32_t id, uint32_t revision, int64_t occurrence, ink_scope_t scope, const char *member, bool adult, uint32_t *undo);
int ink_time_event_restore(uint32_t token, const char *member);
int ink_time_event_next_for(int *cursor, ink_event_t *out, const char *member);
int ink_time_event_get_for(uint32_t id, ink_event_t *out, const char *member);
/* ics.c */
typedef struct { int imported, skipped, existing, full, error; } ink_ics_result_t;
int ink_time_ics_export(const char *member, const char *path, int *count);
int ink_time_ics_import(const char *member, const char *path, ink_ics_result_t *r);

#endif
