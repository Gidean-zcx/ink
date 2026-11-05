#include <string.h>
#include "ink/sys/json.h"
#include "ink/probe.h"
#include "spike.h"

int ink_spike_runtime_start(void) { return 0; }

static int parse(const char *source) {
    ink_json_t *j = ink_spike_alloc(sizeof *j);
    if (!j) return -1;
    int rc = -1;
    if (ink_json_parse(j, source, strlen(source)) != 0) goto done;
    int rows = ink_json_find(j, 0, "rows");
    if (rows < 0 || j->tok[rows].type != 2 || j->tok[rows].size != INK_SPIKE_ROWS ||
        !ink_json_eq(j, ink_json_find(j, 0, "toggle_capability"), "spike.set")) goto done;
    const char *keys[] = {"title", "field_label", "text", "button"};
    char *dst[] = {ink_spike_model->title, ink_spike_model->field_label, ink_spike_model->text, ink_spike_model->button};
    const size_t caps[] = {64, 64, 128, 64};
    for (unsigned i = 0; i < 4; i++)
        if (ink_json_str(j, ink_json_find(j, 0, keys[i]), dst[i], caps[i]) < 0) goto done;
    int t = rows + 1;
    for (unsigned i = 0; i < INK_SPIKE_ROWS; i++) {
        if (t >= j->ntok || j->tok[t].type != 1 ||
            ink_json_str(j, ink_json_find(j, t, "label"), ink_spike_model->rows[i].label, 64) < 0) goto done;
        int v = ink_json_find(j, t, "value");
        if (!ink_json_eq(j, v, "true") && !ink_json_eq(j, v, "false")) goto done;
        ink_spike_model->rows[i].value = ink_json_eq(j, v, "true");
        int end = j->tok[t].end;
        while (t < j->ntok && j->tok[t].start < end) t++;
    }
    ink_spike_model->count = INK_SPIKE_ROWS;
    rc = 0;
done:
    ink_spike_free(j);
    return rc;
}

int ink_spike_build_model(void) { return parse(ink_spike_source()); }
int ink_spike_toggle(unsigned row, bool value, ink_source_t source) {
    return ink_spike_dispatch(row, value, source);
}
void ink_spike_runtime_stop(void) {}
void ink_spike_tests(void) {
    ink_probe_value("spike.bad_json_rejected", parse("{\"rows\":[]}") < 0);
}
