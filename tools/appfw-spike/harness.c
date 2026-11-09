/* A simulator-only experiment; all UI work uses the product UI executor. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spike.h"
#include "ink/sys/textfield.h"
#include "ink/sys/json.h"
#include "sim.h"
#include "pools.h"

#define ink_main ink_product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../../tests/integration/whole_product.h"

static char *source;
static size_t source_size;
static int source_error;
static char snapshot_before[4096], snapshot_after[4096];
static size_t heap_baseline[2], heap_peak[2], tag_peak[3][POOL_COUNT];
static int released;

const char *ink_spike_source(void) { return source ? source : ""; }
static void source_on_host(void *arg) {
    (void)arg;
    const char *path = getenv("INK_SPIKE_SOURCE");
    if (!path) { source_error = 1; return; }
    FILE *f = fopen(path, "rb");
    if (!f) { source_error = 1; return; }
    source_size = fread(source, 1, INK_SPIKE_SOURCE_MAX + 1, f);
    source_error = ferror(f) || source_size > INK_SPIKE_SOURCE_MAX;
    if (fclose(f)) source_error = 1;
    if (!source_error) source[source_size] = 0;
}
static void open_page(void) {
    osal_mem_stats_t st;
    osal_mem_stats(OSAL_MEM_INTERNAL, &st); heap_baseline[0] = st.total - st.min_free;
    osal_mem_stats(OSAL_MEM_EXTERNAL, &st); heap_baseline[1] = st.total - st.min_free;
    ink_ui_push(&ink_spike_activity, NULL);
    ink_spike_free(source); source = NULL;
}
static void capture_before(void) {
    CHECK(ink_ui_snapshot(snapshot_before, sizeof snapshot_before, NULL) > 0, "initial snapshot");
    CHECK(ink_spike_model->count == INK_SPIKE_ROWS, "eight rows");
}
static void ui_checks(void) {
    CHECK(ink_spike_calls == 1 && !ink_spike_model->rows[0].value && !ink_spike_event_errors, "a real tap ran the capability once");
    ink_receipt_t rc;
    CHECK(ink_cap_invoke_json("spike.set", "{\"row\":0,\"value\":true}", INK_SRC_AI, ink_ui_content_version(), &rc) == INK_RC_DONE && ink_spike_calls == 2 &&
          ink_spike_model->rows[0].value && lv_obj_has_state(ink_spike_switches[0], LV_STATE_CHECKED), "AI and touch share the bus");
    uint32_t old = ink_ui_content_version();
    ink_ui_content_changed();
    CHECK(ink_cap_invoke_json("spike.set", "{\"row\":0,\"value\":false}", INK_SRC_AI, old, &rc) == INK_RC_STALE &&
          ink_spike_calls == 2, "stale requests cannot change the page");
    CHECK(ink_cap_invoke_json("spike.set", "{\"row\":8,\"value\":true}", INK_SRC_AI, 0, &rc) == INK_RC_INVALID &&
          ink_spike_calls == 2, "invalid row cannot reach the handler");
    CHECK(ink_cap_invoke("spike.missing", NULL, INK_SRC_AI, 0, &rc) == INK_RC_INVALID, "unknown capability rejected");
    CHECK(ink_ui_snapshot(snapshot_after, sizeof snapshot_after, NULL) > 0, "snapshot after touch and AI restore");
    ink_spike_tests();
}
static void text_visible(void) {
    CHECK(strstr(ink_textfield_text(ink_spike_field), "一起读"), "AI edit visible in the shared field");
}
static void text_checks(void) {
    CHECK(ink_text_focused(), "a tap opened the shared text service");
    CHECK(AI("text.insert", "{\"text\":\"一起读\"}") && g_rc.status == INK_RC_DONE,
          "AI edits through the shared text capability");
    on_ui(text_visible);
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_DONE, "finish editing through text.submit");
}
static void memory_and_close(void) {
    CHECK(ink_spike_submits == 1, "bottom button invoked spike.submit");
    osal_mem_stats_t st;
    osal_mem_stats(OSAL_MEM_INTERNAL, &st); heap_peak[0] = st.total - st.min_free;
    osal_mem_stats(OSAL_MEM_EXTERNAL, &st); heap_peak[1] = st.total - st.min_free;
    for (int i = 0; i < pools_tag_count(); i++) {
        const char *tag;
        size_t cur[POOL_COUNT], peak[POOL_COUNT], live;
        pools_tag_info(i, &tag, cur, peak, &live);
        int at = !strcmp(tag, "appfw.model") ? 0 :
                 (!strcmp(tag, "appfw.lua") || !strcmp(tag, "appfw.mquickjs")) ? 1 : !strcmp(tag, "lvgl") ? 2 : -1;
        if (at >= 0) memcpy(tag_peak[at], peak, sizeof peak);
    }
    ink_ui_pop();
    released = 1;
    for (int i = 0; i < pools_tag_count(); i++) {
        const char *tag;
        size_t cur[POOL_COUNT], peak[POOL_COUNT], live;
        pools_tag_info(i, &tag, cur, peak, &live);
        if ((!strcmp(tag, "appfw.model") || !strcmp(tag, "appfw.lua") || !strcmp(tag, "appfw.mquickjs")) && live) released = 0;
    }
    CHECK(released, "model and VM allocations released when the page closes");
}
static void save_on_host(void *arg) {
    (void)arg;
    char path[600];
    int n = snprintf(path, sizeof path, "%s/spike.json", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f) abort();
    if (fprintf(f, "{\"candidate\":\"%s\",\"startup_us\":%llu,\"build_us\":%llu,\"toggle_us\":%llu,"
                   "\"heap_baseline_internal\":%zu,\"heap_baseline_psram\":%zu,\"heap_peak_internal\":%zu,\"heap_peak_psram\":%zu,"
                   "\"model_peak_psram\":%zu,\"vm_peak_psram\":%zu,\"lvgl_peak_psram\":%zu,\"vm_peak_requested\":%zu,"
                   "\"model_peak_internal\":%zu,\"vm_peak_internal\":%zu,\"lvgl_peak_internal\":%zu,"
                   "\"released\":%s,\"snapshot\":%s,\"snapshot_after\":%s}\n",
                INK_SPIKE_KIND, (unsigned long long)ink_spike_start_us, (unsigned long long)ink_spike_build_us,
                (unsigned long long)ink_spike_toggle_us, heap_baseline[0], heap_baseline[1], heap_peak[0], heap_peak[1],
                tag_peak[0][POOL_EXTERNAL], tag_peak[1][POOL_EXTERNAL], tag_peak[2][POOL_EXTERNAL], ink_spike_vm_peak,
                tag_peak[0][POOL_INTERNAL] + tag_peak[0][POOL_DMA], tag_peak[1][POOL_INTERNAL] + tag_peak[1][POOL_DMA],
                tag_peak[2][POOL_INTERNAL] + tag_peak[2][POOL_DMA],
                released ? "true" : "false", snapshot_before, snapshot_after) < 0 || fclose(f)) abort();
}
static void exercise(void *arg) {
    (void)arg;
    on_ui(open_page);
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 15000), "page became stable");
    on_ui(capture_before);
    sim_display_snapshot("page");
    tap(594, 152);
    CHECK(WAIT_FOR(ink_spike_calls == 1, 3000), "tap reached the switch");
    on_ui(ui_checks);
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "switch result became stable");
    tap(300, 980);
    CHECK(WAIT_FOR(ink_text_focused(), 3000), "text field focused");
    text_checks();
    CHECK(WAIT_FOR(!ink_text_focused(), 3000), "submit closed the shared editor");
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "keyboard closed");
    tap(300, 1130);
    CHECK(WAIT_FOR(ink_spike_submits == 1, 3000), "button ran");
    on_ui(memory_and_close);
    vtk_host_call(save_on_host, NULL);
    vtk_stop();
}
void ink_main(void) {
    if (strcmp(INK_SPIKE_KIND, "c")) {
        source = ink_spike_alloc(INK_SPIKE_SOURCE_MAX + 2);
        if (!source) osal_panic("source allocation failed");
        vtk_host_call(source_on_host, NULL);
        if (source_error || memchr(source, 0, source_size)) osal_panic("source load failed");
    }
    ink_product_main();
    if (ink_spike_register()) osal_panic("spike capability registration failed");
    osal_task_cfg_t cfg = {.name = "appfw-test", .fn = exercise, .stack_bytes = 8192,
                          .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("spike test task failed");
}
