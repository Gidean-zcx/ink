#ifndef INK_APPFW_SPIKE_H
#define INK_APPFW_SPIKE_H

#include "ink/sys/ui.h"
#include "ink/sys/cap.h"

#define INK_SPIKE_ROWS 8
#define INK_SPIKE_SOURCE_MAX 8192

typedef struct {
    char label[64];
    bool value;
} ink_spike_row_t;

typedef struct {
    char title[64], field_label[64], text[128], button[64];
    ink_spike_row_t rows[INK_SPIKE_ROWS];
    unsigned count;
} ink_spike_model_t;

extern ink_spike_model_t *ink_spike_model;
extern lv_obj_t *ink_spike_switches[INK_SPIKE_ROWS], *ink_spike_field, *ink_spike_button;
extern const ink_activity_t ink_spike_activity;
extern uint64_t ink_spike_start_us, ink_spike_build_us, ink_spike_toggle_us;
extern unsigned ink_spike_calls, ink_spike_submits, ink_spike_event_errors;
extern size_t ink_spike_vm_peak;

void *ink_spike_alloc(size_t size);
void ink_spike_free(void *ptr);
const char *ink_spike_source(void);
int ink_spike_runtime_start(void);
int ink_spike_build_model(void);
int ink_spike_toggle(unsigned row, bool value, ink_source_t source);
int ink_spike_dispatch(unsigned row, bool value, ink_source_t source);
void ink_spike_runtime_stop(void);
int ink_spike_register(void);
void ink_spike_tests(void);

#endif
