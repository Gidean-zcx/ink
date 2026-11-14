/* The calendar app (calendar-3): its pages are cal_*.c. This file stays the entry the build lists for translation
 * checks; the new wording waits in docs/tasks/calendar-3/I18N-ADD.tsv for the main session. */
#include "cal.h"

int cal_init(void);
int ink_calendar_init(void) { return cal_init(); }
