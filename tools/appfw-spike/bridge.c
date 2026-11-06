#include <string.h>
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/font.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/textfield.h"
#include "spike.h"
#include "vtk.h"

ink_spike_model_t *ink_spike_model;
lv_obj_t *ink_spike_switches[INK_SPIKE_ROWS], *ink_spike_field, *ink_spike_button;
uint64_t ink_spike_start_us, ink_spike_build_us, ink_spike_toggle_us;
unsigned ink_spike_calls, ink_spike_submits, ink_spike_event_errors;
size_t ink_spike_vm_peak;
static const lv_font_t *font;

/* Commit host CPU accounting before reading each interval boundary. */
static uint64_t boundary(void) { vtk_sched_point(); return osal_now_us(); }

void *ink_spike_alloc(size_t size) {
    const char *prev = osal_mem_tag("appfw.model");
    void *p = osal_calloc(1, size, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    return p;
}
void ink_spike_free(void *ptr) { osal_free(ptr); }

static void set_row(ink_call_t *call) {
    int64_t row;
    bool value;
    if (!ink_args_get_int(&call->args, "row", &row) || row < 0 || row >= INK_SPIKE_ROWS ||
        !ink_args_get_bool(&call->args, "value", &value)) { call->rc.status = INK_RC_INVALID; return; }
    ink_spike_model->rows[row].value = value;
    if (value) lv_obj_add_state(ink_spike_switches[row], LV_STATE_CHECKED);
    else lv_obj_remove_state(ink_spike_switches[row], LV_STATE_CHECKED);
    ink_spike_calls++;
    ink_ui_content_changed();
    ink_ui_hint_obj(ink_spike_switches[row], INK_REFRESH_FAST);
}
static void submit(ink_call_t *call) {
    (void)call;
    ink_spike_submits++;
    lv_label_set_text(lv_obj_get_child(ink_spike_button, 0), "已完成");
    ink_ui_content_changed();
}
static const ink_cap_t caps[] = {
    {.id = "spike.set", .title = "更改阅读偏好", .effect = INK_FX_LOCAL_WRITE,
     .activity = "appfw-spike", .child_ok = true, .run = set_row,
     .input = "{\"type\":\"object\",\"properties\":{\"row\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7},\"value\":{\"type\":\"boolean\"}},\"required\":[\"row\",\"value\"],\"additionalProperties\":false}"},
    {.id = "spike.submit", .title = "完成", .effect = INK_FX_LOCAL_WRITE,
     .activity = "appfw-spike", .child_ok = true, .run = submit}
};
int ink_spike_register(void) { return ink_cap_register(caps, sizeof caps / sizeof caps[0]); }
int ink_spike_dispatch(unsigned row, bool value, ink_source_t source) {
    ink_args_t args;
    ink_args_init(&args);
    if (ink_args_int(&args, "row", row) || ink_args_bool(&args, "value", value)) return -1;
    ink_receipt_t rc;
    return ink_cap_invoke("spike.set", &args, source, ink_ui_content_version(), &rc) == INK_RC_DONE ? 0 : -1;
}
static void changed(lv_event_t *event) {
    unsigned row = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    bool value = lv_obj_has_state(ink_spike_switches[row], LV_STATE_CHECKED);
    uint64_t start = boundary();
    if (ink_spike_toggle(row, value, INK_SRC_TOUCH)) ink_spike_event_errors++;
    ink_spike_toggle_us = boundary() - start;
}
static void pressed(lv_event_t *event) {
    (void)event;
    ink_receipt_t rc;
    if (ink_cap_invoke("spike.submit", NULL, INK_SRC_TOUCH, 0, &rc) != INK_RC_DONE) ink_spike_event_errors++;
}
static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_label_set_text(obj, text);
    lv_obj_set_pos(obj, x, y);
    return obj;
}
static void entered(void *ctx, const char *value) {
    (void)ctx;
    (void)value;
    ink_text_blur(0);
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    uint64_t start = boundary();
    ink_spike_model = ink_spike_alloc(sizeof *ink_spike_model);
    if (!ink_spike_model || ink_spike_runtime_start()) osal_panic("spike runtime start failed");
    ink_spike_start_us = boundary() - start;
    start = boundary();
    if (ink_spike_build_model()) osal_panic("spike model build failed");
    ink_font_t *cjk = ink_font_open("fonts/sans-44.inkf");
    if (!cjk) osal_panic("spike CJK font missing");
    font = ink_font_lvgl(cjk);
    ink_ui_style_screen(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    label(screen, ink_spike_model->title, 32, 32);
    for (unsigned i = 0; i < INK_SPIKE_ROWS; i++) {
        label(screen, ink_spike_model->rows[i].label, 32, 128 + (int)i * 96);
        lv_obj_t *s = ink_spike_switches[i] = lv_switch_create(screen);
        lv_obj_set_pos(s, 536, 120 + (int)i * 96);
        lv_obj_set_size(s, 116, 64);
        lv_obj_set_style_anim_duration(s, 0, 0);
        lv_obj_set_style_bg_color(s, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s, 3, 0);
        lv_obj_set_style_border_color(s, lv_color_black(), 0);
        lv_obj_set_style_bg_color(s, lv_color_white(), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s, lv_color_black(), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s, lv_color_black(), LV_PART_KNOB);
        lv_obj_set_style_bg_color(s, lv_color_white(), LV_PART_KNOB | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
        lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
        lv_obj_set_style_pad_all(s, -6, LV_PART_KNOB);
        if (ink_spike_model->rows[i].value) lv_obj_add_state(s, LV_STATE_CHECKED);
        lv_obj_add_event_cb(s, changed, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);
    }
    label(screen, ink_spike_model->field_label, 32, 900);
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = ink_spike_model->text, .max = 120};
    ink_spike_field = ink_textfield_create(screen, &cfg, entered, NULL, false);
    if (!ink_spike_field) osal_panic("spike text field failed");
    lv_obj_set_pos(ink_spike_field, 32, 944);
    lv_obj_set_size(ink_spike_field, 620, 84);
    ink_spike_button = lv_button_create(screen);
    ink_ui_style_button(ink_spike_button);
    lv_obj_set_pos(ink_spike_button, 32, 1080);
    lv_obj_set_size(ink_spike_button, 620, 104);
    lv_obj_t *text = label(ink_spike_button, ink_spike_model->button, 0, 0);
    lv_obj_center(text);
    lv_obj_add_event_cb(ink_spike_button, pressed, LV_EVENT_CLICKED, NULL);
    lv_obj_update_layout(screen);
    ink_spike_build_us = boundary() - start;
}
static void destroy(void) {
    ink_spike_runtime_stop();
    ink_spike_free(ink_spike_model);
    ink_spike_model = NULL;
}
static int snapshot(char *buf, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, buf, cap);
    ink_jw_raw(&w, "{\"page\":\"appfw-spike\",\"member\":");
    ink_jw_str(&w, ink_member_current()->id);
    ink_jw_raw(&w, ",\"version\":"); ink_jw_int(&w, ink_ui_content_version());
    ink_jw_raw(&w, ",\"title\":");
    ink_jw_str(&w, ink_spike_model->title);
    ink_jw_raw(&w, ",\"rows\":[");
    for (unsigned i = 0; i < INK_SPIKE_ROWS; i++) {
        if (i) ink_jw_raw(&w, ",");
        ink_jw_raw(&w, "{\"id\":"); ink_jw_int(&w, i);
        ink_jw_raw(&w, ",\"label\":"); ink_jw_str(&w, ink_spike_model->rows[i].label);
        ink_jw_raw(&w, ink_spike_model->rows[i].value ? ",\"value\":true}" : ",\"value\":false}");
    }
    ink_jw_raw(&w, "],\"field\":{\"id\":\"note\",\"role\":\"textbox\",\"label\":");
    ink_jw_str(&w, ink_spike_model->field_label);
    ink_jw_raw(&w, ",\"value\":"); ink_jw_str(&w, ink_textfield_text(ink_spike_field));
    ink_jw_raw(&w, "},\"button\":"); ink_jw_str(&w, ink_spike_submits ? "已完成" : ink_spike_model->button);
    ink_jw_raw(&w, ",\"actions\":[\"spike.set\",\"text.insert\",\"text.submit\",\"spike.submit\"]}");
    return w.overflow ? -1 : (int)w.len;
}
const ink_activity_t ink_spike_activity = {.name = "appfw-spike", .create = create, .destroy = destroy, .snapshot = snapshot};
