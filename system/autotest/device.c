/* Test-image-only glue: keep the normal boot, input driver and policy unchanged. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "script.h"
#include "ink/osal.h"
#include "ink/rotation.h"
#include "ink/hal_display.h"
#include "ink/sys/confirm.h"
#include "ink/sys/display.h"
#include "ink/sys/log.h"
#include "ink/sys/textinput.h"
#include "ink/sys/ui.h"
#include "autotest_script.h"

static hal_input_sink_fn input_sink;
static void *input_ctx;
static bool guarded, down;
static int last_x, last_y, finished;
static ink_exec_job_t job;
static ink_script_t script;
static hal_panel_info_t panel;

void __real_hal_input_set_sink(hal_input_sink_fn fn, void *ctx);
void __wrap_hal_input_set_sink(hal_input_sink_fn fn, void *ctx) {
    input_sink = fn; input_ctx = ctx;
    __real_hal_input_set_sink(fn, ctx);
}
static bool denied(const char *id) {
    bool no = __atomic_load_n(&guarded, __ATOMIC_ACQUIRE) && !ink_script_cap_allowed(ink_cap_find(id));
    if (no) ink_log(INK_LOG_WARN, "autotest", "autotest: 0 cap rejected source=test");
    return no;
}
ink_rc_t __real_ink_cap_invoke(const char *, const ink_args_t *, ink_source_t, uint32_t, ink_receipt_t *);
ink_rc_t __real_ink_cap_invoke_json(const char *, const char *, ink_source_t, uint32_t, ink_receipt_t *);
int __real_ink_cap_call(const char *, const ink_args_t *, const ink_call_opts_t *);
static ink_rc_t rejected(ink_receipt_t *rc) { memset(rc, 0, sizeof *rc); return rc->status = INK_RC_REJECTED; }
ink_rc_t __wrap_ink_cap_invoke(const char *id, const ink_args_t *args, ink_source_t src, uint32_t gen, ink_receipt_t *rc) {
    return denied(id) ? rejected(rc) : __real_ink_cap_invoke(id, args, src, gen, rc);
}
ink_rc_t __wrap_ink_cap_invoke_json(const char *id, const char *args, ink_source_t src, uint32_t gen, ink_receipt_t *rc) {
    return denied(id) ? rejected(rc) : __real_ink_cap_invoke_json(id, args, src, gen, rc);
}
int __wrap_ink_cap_call(const char *id, const ink_args_t *args, const ink_call_opts_t *o) {
    return denied(id) ? -1 : __real_ink_cap_call(id, args, o);
}
int __real_ink_confirm_decide(uint32_t token, bool accept, ink_source_t src);
int __wrap_ink_confirm_decide(uint32_t token, bool accept, ink_source_t src) {
    return __atomic_load_n(&guarded, __ATOMIC_ACQUIRE) && accept ? -1 : __real_ink_confirm_decide(token, accept, src);
}
/* Runs on UI; the captured service sink feeds the very same UI input queue as hardware. */
static void dispatch(void *arg) {
    action_t *a = arg;
    int result = 0;
    if (!a) result = !strcmp(ink_ui_page_name(), "home") && ink_display_commits() && !ink_display_busy() ? 0 : -1;
    else if (ink_text_focused() && (ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden())) result = -1;
    else if (a->kind == A_CAP) {
        const ink_cap_t *c = ink_cap_find(a->text);
        ink_receipt_t rc;
        result = !ink_script_cap_allowed(c) ? -1 : (int)ink_cap_invoke_json(a->text, a->path, INK_SRC_APP, 0, &rc);
        ink_log(INK_LOG_INFO, "autotest", "autotest: %d cap source=test result=%d", a->line, result);
        result = result == INK_RC_DONE || result == INK_RC_QUEUED ? 0 : -1;
    } else if (input_sink) {
        hal_input_event_t h = {.t_us = osal_now_us()};
        if (a->kind == A_KEY) {
            h.type = HAL_IN_KEY; h.key.key = a->a; h.key.pressed = a->b;
            if (a->a == HAL_KEY_AI) result = -1;
        } else {
            h.type = HAL_IN_TOUCH;
            h.touch.phase = a->a ? HAL_TOUCH_UP : down ? HAL_TOUCH_MOVE : HAL_TOUCH_DOWN;
            down = !a->a;
            if (a->b >= 0) { last_x = a->b; last_y = a->c; }
            ink_logical_to_native(panel.width, panel.height, panel.rotation_deg, last_x, last_y, &h.touch.x, &h.touch.y);
        }
        if (!result) input_sink(&h, input_ctx);
    } else result = -1;
    __atomic_store_n(&finished, result ? -1 : 1, __ATOMIC_RELEASE);
}
static int on_ui(action_t *a) {
    __atomic_store_n(&finished, 0, __ATOMIC_RELEASE);
    if (ink_exec_post(INK_EXEC_UI, &job, dispatch, a)) return -1;
    while (!__atomic_load_n(&finished, __ATOMIC_ACQUIRE)) osal_task_delay_us(1000);
    return finished < 0 ? -1 : 0;
}
static void *resize(void *p, size_t n) { return osal_realloc(p, n, OSAL_MEM_EXTERNAL); }
static void run(void *unused) {
    (void)unused;
    unsigned executed = 0, skipped = 0;
    uint64_t boot = osal_now_us();
    while (on_ui(NULL)) {
        if (osal_now_us() - boot >= 60000000ull) {
            ink_log(INK_LOG_ERROR, "autotest", "autotest: summary executed=0 skipped=0 elapsed_ms=60000 home-timeout");
            return;
        }
        osal_task_delay_us(100000);
    }
    hal_display_get_info(&panel);
    ink_logical_size(panel.width, panel.height, panel.rotation_deg, &script.width, &script.height);
    script.dpi = panel.dpi; script.resize = resize;
    char *line = osal_malloc(512, OSAL_MEM_EXTERNAL);
    if (!line) return;
    const char *p = ink_autotest_script;
    int lineno = 0;
    while (*p) {
        const char *end = strchr(p, '\n');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        if (n >= 512) { skipped++; ink_log(INK_LOG_WARN, "autotest", "autotest: %d parse-error", ++lineno); }
        else {
            memcpy(line, p, n); line[n] = 0;
            char error[80] = "";
            if (ink_script_line(&script, line, ++lineno, error, sizeof error)) {
                skipped++; ink_log(INK_LOG_WARN, "autotest", "autotest: %d parse-error", lineno);
            }
        }
        p += n + (end != NULL);
    }
    osal_free(line);
    /* Stable order, including release edges that overlap the next script line. */
    for (size_t i = 1; i < script.count; i++) {
        action_t value = script.actions[i]; size_t j = i;
        while (j && script.actions[j - 1].t > value.t) { script.actions[j] = script.actions[j - 1]; j--; }
        script.actions[j] = value;
    }
    uint64_t start = osal_now_us();
    for (size_t i = 0; i < script.count; i++) {
        action_t *a = &script.actions[i];
        uint64_t due = start + a->t / 1000;
        uint64_t now = osal_now_us();
        if (due > now) osal_task_delay_us(due - now);
        bool first = true;
        for (size_t j = 0; j < i; j++) if (script.actions[j].line == a->line) first = false;
        int result = 0;
        if (a->kind == A_TOUCH || a->kind == A_KEY || a->kind == A_CAP) result = on_ui(a);
        else if (a->kind != A_END && a->kind != A_NOTE && a->kind != A_MARK) result = -1;
        if (first) {
            if (result) skipped++; else executed++;
            ink_log(INK_LOG_INFO, "autotest", "autotest: %d %.11s %s", a->line, a->verb, result ? "skipped/rejected" : "executed");
        }
        if (a->kind == A_END) break;
    }
    ink_log(INK_LOG_INFO, "autotest", "autotest: summary executed=%u skipped=%u elapsed_ms=%llu", executed, skipped,
        (unsigned long long)((osal_now_us() - start) / 1000));
    osal_free(script.actions); script.actions = NULL;
}
void __real_ink_main(void);
void __wrap_ink_main(void) {
    __real_ink_main();
    __atomic_store_n(&guarded, true, __ATOMIC_RELEASE);
    const osal_task_cfg_t cfg = {.name = "autotest", .fn = run, .stack_bytes = 4096,
        .priority = OSAL_PRIO_LOW, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) ink_log(INK_LOG_ERROR, "autotest", "autotest: task-create-failed");
}
