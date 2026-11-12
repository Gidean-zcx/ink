/* Continuous real memo edits, storage deadlines and the panel guard. */
#include "ink/hal_display.h"
static unsigned refreshes, guarded_refreshes, guard_refreshes, guard_count, flushed;
static bool observing, released;
static uint32_t checked_update(const hal_rect_t *, const uint8_t *, int, hal_waveform_t, uint32_t, hal_display_done_fn, void *);
#define hal_display_update checked_update
#include "../system/services/display/display_service.c"
#undef hal_display_update
static uint32_t checked_update(const hal_rect_t *r, const uint8_t *px, int stride, hal_waveform_t wf,
                               uint32_t flags, hal_display_done_fn done, void *ctx) {
    refreshes++;
    if (observing && released) flushed++;
    if (DS.guard) guarded_refreshes++;
    return hal_display_update(r, px, stride, wf, flags, done, ctx);
}
static void checked_guard(bool hold) {
    if (!hold && observing && refreshes != guard_refreshes) guarded_refreshes++;
    ink_display_flash_guard(hold);
    if (hold) {
        guard_refreshes = refreshes;
        if (observing) guard_count++;
    } else if (observing) released = true;
}
#define ink_display_flash_guard checked_guard
#define request storage_request
#include "../system/services/storage/storage_service.c"
#undef request
#undef ink_display_flash_guard
#include "../system/apps/memo/memo.c"
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "ink/rotation.h"
#include <stdlib.h>

static lv_timer_t *typing;
static unsigned typed, typing_errors, pixel_errors, pixels_checked;
static char save_json[160], final_text[512];
static uint8_t *visible;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void type_tick(lv_timer_t *tick) {
    (void)tick; ink_receipt_t rc;
    if (ink_cap_invoke_json("keyboard.key", "{\"code\":120}", INK_SRC_TOUCH, ink_ui_content_version(), &rc) != INK_RC_DONE) typing_errors++;
    typed++;
}
static void start_typing(void) { typing = lv_timer_create(type_tick, 50, NULL); }
static void stop_typing(void) { lv_timer_delete(typing); typing = NULL; }
static void prepare(void) {
    if (!ink_text_english()) ink_text_key(INK_TEXT_KEY_LANGUAGE);
    snprintf(save_json, sizeof save_json, "{\"id\":%u,\"draft\":%u,\"page\":\"%s\"}", (unsigned)M.id, (unsigned)M.note.draft, M.note.page);
}
static void inspect_final(void) {
    snprintf(final_text, sizeof final_text, "%s", ink_textfield_text(M.field));
    CHECK(strlen(final_text) == typed && !typing_errors, "all continuous keys reach memo: typed=%u bytes=%zu errors=%u", typed, strlen(final_text), typing_errors);
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "final memo text has an AI snapshot");
}
static void compare_pixels(void *unused) {
    (void)unused; int lw, lh;
    if (!sim_display_visible(visible, (size_t)DS.w * DS.h, &lw, &lh)) abort();
    for (int y = 0; y < lh; y++) for (int x = 0; x < lw; x++) {
        int nx, ny; ink_logical_to_native(DS.w, DS.h, DS.info.rotation_deg, x, y, &nx, &ny);
        uint8_t target = DS.fb[(size_t)ny * DS.stride + nx];
        if (target != 0 && target != 255) continue; /* Antialiasing and optical residue are modelled separately. */
        pixels_checked++;
        if ((visible[(size_t)y * lw + x] < 128) != (target < 128)) pixel_errors++;
    }
    sim_display_snapshot("final-memo");
}
static void final_saved(void) {
    memo_t *note = ink_app_alloc(sizeof *note, true);
    CHECK(note && read_note(M.db, M.id, note) && !strcmp(note->body, final_text), "saved memo and final text agree");
    ink_app_free(note);
}
static void run(void *unused) {
    (void)unused;
    CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "boot settles"); test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    CHECK(BY_HAND("memo.edit", NULL) && WAIT_FOR(ink_text_focused() && !M.saving && ink_db_idle() && !ink_storage_pending() && settled(), 15000), "memo opens and initial save completes");
    on_ui(prepare); CHECK(WAIT_FOR(settled(), 15000), "language change settles");
    uint64_t typing_until = osal_now_us() + 10000000;
    observing = true; on_ui(start_typing); delay_ms(800);
    CHECK(ink_display_busy(), "continuous typing has a refresh in flight");
    uint64_t begin = osal_now_us();
    CHECK(BY_HAND("memo.save", save_json), "memo save replies while typing continues");
    CHECK(g_rc.status == INK_RC_DONE && osal_now_us() - begin < 3000000, "busy memo save completes within three seconds: rc=%d time=%llu us", g_rc.status, (unsigned long long)(osal_now_us() - begin));
    ink_probe_value("test.memo_save_us", (double)(osal_now_us() - begin));
    CHECK(guard_count > 0 && !guarded_refreshes, "real storage guards forbid new refreshes");
    CHECK(WAIT_FOR(flushed > 0, 3000), "storage guard release flushes queued refreshes");
    if (typing_until > osal_now_us()) osal_task_delay_us(typing_until - osal_now_us());
    on_ui(stop_typing);
    CHECK(WAIT_FOR(!M.saving && M.field && !strcmp(M.sent, ink_textfield_text(M.field)) && ink_db_idle() && !ink_storage_pending() && settled(), 15000), "final save and queued pixels settle");
    on_ui(inspect_final); on_ui(final_saved);
    visible = osal_malloc((size_t)DS.w * DS.h, OSAL_MEM_EXTERNAL);
    CHECK(visible != NULL, "allocate final panel comparison in PSRAM");
    if (visible) vtk_host_call(compare_pixels, NULL);
    CHECK(pixels_checked > 700000 && !pixel_errors, "final panel matches solid text pixels: checked=%u wrong=%u", pixels_checked, pixel_errors);
    osal_free(visible);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "memo-busy-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start busy memo acceptance");
}
