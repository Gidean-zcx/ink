/* Shared timed-script parser; timestamps are nanoseconds, coordinates logical. */
#ifndef INK_SCRIPT_H
#define INK_SCRIPT_H
#include <stddef.h>
#include <stdint.h>
#include "ink/hal_input.h"
#include "ink/sys/cap.h"
#define INK_SCRIPT_MS(n) ((uint64_t)(n) * 1000000ull)
typedef enum { A_TOUCH, A_KEY, A_SNAP, A_MARK, A_NOTE, A_BATTERY, A_POWERCUT, A_END, A_NET, A_OUTAGE, A_MIC, A_MIC_SAY, A_SIM, A_ORIENT, A_SHAKE, A_RTC_LOST, A_SMS_IN, A_CALL_IN, A_REMOTE, A_ENV, A_FAULTS, A_SD, A_USB, A_HUB_CLOCK, A_RTC_LINE, A_HEADSET, A_AP, A_CAP, A_STALL } akind_t;

typedef struct {
    uint64_t t;
    akind_t kind;
    int a, b, c;
    char text[96];
    char num[24];
    char path[512];
    int line;
    char verb[12];
    ink_exec_job_t job;
} action_t;


typedef struct {
    action_t *actions;
    size_t count, capacity;
    uint64_t previous;
    int width, height, dpi, line;
    const char *verb;
    bool has_end;
    void *(*resize)(void *, size_t);
} ink_script_t;
int ink_script_line(ink_script_t *s, char *line, int lineno, char *err, size_t errlen);
bool ink_script_cap_allowed(const ink_cap_t *cap);
#endif
