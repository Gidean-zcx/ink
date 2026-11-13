/* iCalendar import and export (calendar-3): placeholder until the reader and writer land. */
#include "time_internal.h"

int ink_time_ics_export(const char *member, const char *path, int *count) { (void)member; (void)path; if (count) *count = 0; return -1; }
int ink_time_ics_import(const char *member, const char *path, ink_ics_result_t *r) { (void)member; (void)path; (void)r; return -1; }
