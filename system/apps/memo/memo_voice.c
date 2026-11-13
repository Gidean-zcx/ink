/* MemoRecord: the copied ZSWatch recording screen (vendor/zswatch, GPL-3.0) wired to the media service. UI task. */
#include <stdio.h>
#include <string.h>
#include "ink/sys/cap.h"
#include "ink/sys/i18n.h"
#include "ink/sys/media.h"
#include "ink/sys/state.h"
#include "ink/ui_shell.h"
#include "voice_memo_ui.h"

#define GUARD_MS 2000           /* voice_memo_app.c RECORD_TAP_GUARD_MS: a second tap on stop starts nothing */
#define REC_MAX_S (6 * 3600)    /* the media service's longest recording */
#define CARD_KB_PER_S ((24000 / 8 + 1023) / 1024) /* voice_memo_app.c:200-201 with our 24 kbit/s Opus */
static struct { lv_obj_t *cancel; lv_timer_t *timer; uint32_t started, minute, stopped; bool watched; } V;

static uint32_t remaining_s(uint32_t elapsed_s) {
    int64_t free_bytes = 0; ink_state_get_int(ink_state_find("storage.free"), &free_bytes);
    uint32_t card = (uint32_t)(free_bytes / 1024 / CARD_KB_PER_S), left = elapsed_s < REC_MAX_S ? REC_MAX_S - elapsed_s : 0;
    return card < left ? card : left;
}
static void tick(lv_timer_t *t) { /* every 5 s; the time line changes once a minute */
    uint32_t elapsed = lv_tick_elaps(V.started);
    if (t && elapsed / 60000 == V.minute) return;
    V.minute = elapsed / 60000; voice_memo_ui_update_time(elapsed, remaining_s(elapsed / 1000));
}
static void saving(void) { voice_memo_ui_show_saving(); if (V.cancel) lv_obj_add_flag(V.cancel, LV_OBJ_FLAG_HIDDEN); }
static void stopped(void) { /* the recording ended, by whoever: the record_done event takes the page to MemoPending */
    if (V.timer) { lv_timer_delete(V.timer); V.timer = NULL; saving(); }
    V.started = 0;
}
static void recording(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; if (!ink_audio_recording()) stopped(); }
void ink_memo_voice_stop(void) { /* 记好了 and the back key: the same capability the AI calls */
    ink_receipt_t rc; V.stopped = lv_tick_get();
    if (ink_cap_invoke_json("audio.record_stop", "{}", INK_SRC_TOUCH, 0, &rc) == INK_RC_DONE) stopped();
}
static const voice_memo_ui_callbacks_t CALLBACKS = {.on_stop_recording = ink_memo_voice_stop};
void ink_memo_voice_status(char *out, size_t cap) { /* what the screen says, for the page snapshot */
    uint32_t elapsed = lv_tick_elaps(V.started), left = remaining_s(elapsed / 1000) / 60;
    if (!V.timer) snprintf(out, cap, "%s", TR("正在保存录音"));
    else if (V.minute) snprintf(out, cap, TR("%u 分钟"), (unsigned)V.minute);
    else snprintf(out, cap, "%s", TR("不到 1 分钟"));
    char rest[64]; snprintf(rest, sizeof rest, TR("还能录约 %u 分钟"), (unsigned)left);
    size_t n = strlen(out);
    if (V.timer && left && n < cap) snprintf(out + n, cap - n, " · %s", rest);
}
bool ink_memo_voice_guarded(void) { return V.stopped && lv_tick_elaps(V.stopped) < GUARD_MS; }
bool ink_memo_voice_resume(void) { return V.started && ink_audio_recording(); }
void ink_memo_voice_create(lv_obj_t *screen) {
    if (!V.watched) V.watched = ink_state_watch("audio.recording", recording, NULL, INK_EXEC_UI) == 0;
    lv_obj_t *content = ink_ui_box(screen, 0, INK_STATUS_BAR, INK_CANVAS_W, INK_CANVAS_H - INK_STATUS_BAR, 0, INK_SURFACE_WHITE, false);
    voice_memo_ui_show(content, &CALLBACKS);
    voice_memo_ui_show_recording();
    if (!ink_memo_voice_resume()) V.started = lv_tick_get();
    int w = (INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP) * 3 / 7;
    V.cancel = ink_ui_pill(content, TR("不要了"), INK_MARGIN, INK_CANVAS_H - INK_STATUS_BAR - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, false, NULL, NULL);
    ink_shell_bind(V.cancel, "audio.record_cancel", NULL);
    tick(NULL); V.timer = lv_timer_create(tick, 5000, NULL);
    if (!ink_audio_recording()) stopped();
}
void ink_memo_voice_destroy(void) {
    if (V.timer) lv_timer_delete(V.timer);
    V.timer = NULL; V.cancel = NULL; voice_memo_ui_remove();
}
