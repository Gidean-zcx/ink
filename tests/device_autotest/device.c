/* Fake clock and executor; the device runner, parser and all wrappers are production code. */
#include <assert.h>
#include <stdarg.h>
#include "../../system/autotest/device.c"
static uint64_t now;
static bool ui, booted, secret;
static unsigned inputs, touch_downs, touch_moves, touch_ups, key_edges, invocations, checks, releases;
static uint64_t first_input, held;
static char logs[16000];
static osal_task_cfg_t task;
static const ink_cap_t caps[] = {
    {.id = "app.open", .effect = INK_FX_NAVIGATE},
    {.id = "phone.emergency_call", .effect = INK_FX_NAVIGATE},
    {.id = "file.delete", .effect = INK_FX_NAVIGATE},
    {.id = "member.set_pin", .effect = INK_FX_LOCAL_WRITE},
    {.id = "test.high", .effect = INK_FX_SYSTEM}
};
void ink_log(ink_log_level_t level, const char *module, const char *fmt, ...) {
    (void)level; (void)module;
    va_list args; va_start(args, fmt);
    size_t n = strlen(logs);
    int written = vsnprintf(logs + n, sizeof logs - n, fmt, args);
    va_end(args);
    assert(written >= 0 && (size_t)written + 1 < sizeof logs - n);
    strcat(logs, "\n");
}
uint64_t osal_now_us(void) { return now; }
void osal_task_delay_us(uint64_t us) { assert(!ui); now += us; }
void *osal_realloc(void *p, size_t n, uint32_t flags) { assert(flags == OSAL_MEM_EXTERNAL); return realloc(p, n); }
void *osal_malloc(size_t n, uint32_t flags) { assert(flags == OSAL_MEM_EXTERNAL); return malloc(n); }
void osal_free(void *p) { free(p); }
osal_err_t osal_task_create(const osal_task_cfg_t *cfg, osal_task_t **out) {
    (void)out; assert(booted); task = *cfg; return OSAL_OK;
}
int ink_exec_post(ink_exec_t executor, ink_exec_job_t *node, void (*fn)(void *), void *arg) {
    (void)node; assert(executor == INK_EXEC_UI); ui = true; fn(arg); ui = false; return 0;
}
const char *ink_ui_page_name(void) { assert(ui); return ++checks < 3 ? "boot" : "home"; }
uint32_t ink_display_commits(void) { return 1; }
int ink_display_busy(void) { return 0; }
void hal_display_get_info(hal_panel_info_t *out) {
    *out = (hal_panel_info_t){.width = 1216, .height = 684, .rotation_deg = 90, .dpi = 297};
}
bool ink_text_focused(void) { return secret; }
ink_text_type_t ink_text_type(void) { return INK_TEXT_PASSWORD; }
bool ink_text_ai_hidden(void) { return secret; }
const ink_cap_t *ink_cap_find(const char *id) {
    for (size_t i = 0; i < sizeof caps / sizeof *caps; i++) if (!strcmp(id, caps[i].id)) return &caps[i];
    return NULL;
}
ink_rc_t __real_ink_cap_invoke_json(const char *id, const char *json, ink_source_t src, uint32_t gen, ink_receipt_t *rc) {
    (void)gen; assert(ui && !strcmp(id, "app.open")); assert(src == INK_SRC_APP);
    assert(strstr(json, "FAKE # PRIVATE")); invocations++; return rc->status = INK_RC_DONE;
}
ink_rc_t ink_cap_invoke_json(const char *id, const char *json, ink_source_t src, uint32_t gen, ink_receipt_t *rc) {
    return __wrap_ink_cap_invoke_json(id, json, src, gen, rc);
}
ink_rc_t __real_ink_cap_invoke(const char *id, const ink_args_t *args, ink_source_t src, uint32_t gen, ink_receipt_t *rc) {
    (void)id; (void)args; (void)src; (void)gen; (void)rc; abort();
}
int __real_ink_cap_call(const char *id, const ink_args_t *args, const ink_call_opts_t *opts) {
    (void)id; (void)args; (void)opts; abort();
}
int __real_ink_confirm_decide(uint32_t token, bool accept, ink_source_t src) {
    (void)token; (void)src; assert(!accept); return 0;
}
static void input(const hal_input_event_t *event, void *ctx) {
    (void)ctx; assert(ui); inputs++;
    if (!first_input) first_input = now;
    if (event->type == HAL_IN_TOUCH) {
        if (event->touch.phase == HAL_TOUCH_DOWN) {
            touch_downs++; held = now;
            if (touch_downs == 1) {
                int x, y; ink_native_to_logical(1216, 684, 90, event->touch.x, event->touch.y, &x, &y);
                assert(x == 342 && y == 304);
            }
        } else if (event->touch.phase == HAL_TOUCH_UP) {
            touch_ups++;
            if (touch_ups == 2) assert(now - held >= 800000);
        } else touch_moves++;
    } else { assert(event->key.key == HAL_KEY_HOME); key_edges++; if (!event->key.pressed) releases++; }
}
void __real_hal_input_set_sink(hal_input_sink_fn fn, void *ctx) { assert(fn == input && ctx == NULL); }
void __real_ink_main(void) { booted = true; __wrap_hal_input_set_sink(input, NULL); }
int main(void) {
    __wrap_ink_main();
    assert(task.fn && task.stack_bytes == 4096 && task.flags == OSAL_TASK_STACK_EXTERNAL && task.priority == OSAL_PRIO_LOW);
    task.fn(NULL);
    assert(first_input >= 200000 && touch_downs == 4 && touch_ups == 4 && touch_moves == 11 && key_edges == 2 && releases == 1);
    assert(invocations == 1 && script.actions == NULL);
    assert(strstr(logs, "summary executed=11 skipped=10 elapsed_ms=1620"));
    assert(strstr(logs, "21 parse-error") && strstr(logs, "12 cap source=test result=-1"));
    assert(!strstr(logs, "FAKE_PRIVATE") && !strstr(logs, "FAKE #") && !strstr(logs, "FAKE_PIN"));
    ink_receipt_t rc;
    assert(__wrap_ink_cap_invoke("phone.emergency_call", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_REJECTED);
    assert(__wrap_ink_cap_call("file.delete", NULL, NULL) == -1);
    assert(__wrap_ink_confirm_decide(1, true, INK_SRC_TOUCH) == -1);
    assert(__wrap_ink_confirm_decide(1, false, INK_SRC_TOUCH) == 0);
    unsigned before = inputs;
    action_t a = {.kind = A_TOUCH}; secret = true; assert(on_ui(&a) == -1); assert(inputs == before);
    puts(logs);
    puts("autotest device: passed");
}
