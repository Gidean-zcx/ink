/* Voice memos on the whole product: the real memo app, media service, AI sheet and the hub's stand-in dictation.
 * INK_MR_PHASE: main, cut (a power cut while recording), after (the boot after it), cost (one minute, for D-112). */
#include "../../system/apps/memo/memo.c"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../../system/services/audio/ogg.h"
#include "../integration/whole_product.h"
#include <stdlib.h>

static char phase[16], snapshot_json[12000], scene[32];
static const char *wanted;
static lv_obj_t *found;
static void host_phase(void *unused) { (void)unused; snprintf(phase, sizeof phase, "%s", getenv("INK_MR_PHASE")); }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "display settles"); }
static void shot(void *unused) { (void)unused; sim_display_snapshot(scene); }
static void take_snapshot(void) { CHECK(ink_ui_snapshot(snapshot_json, sizeof snapshot_json, NULL) > 0, "page %s has an AI snapshot", scene); }
static void snap(const char *name) { /* the AI snapshot, and a picture of the panel in the test output */
    snprintf(scene, sizeof scene, "%s", name); quiet(); on_ui(take_snapshot);
    vtk_event_at(vtk_now() + SIM_MS(1), shot, NULL); delay_ms(5);
}
static void find(lv_obj_t *obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), wanted)) found = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) find(lv_obj_get_child(obj, i));
}
static void locate(void) {
    found = NULL; find(lv_screen_active()); find(lv_layer_sys());
    if (found) { lv_area_t r; lv_obj_t *b = lv_obj_get_parent(found); lv_obj_update_layout(b); lv_obj_get_coords(b, &r); g_btn_x[0] = (r.x1 + r.x2) / 2; g_btn_y[0] = (r.y1 + r.y2) / 2; }
}
static bool touch(const char *label) { wanted = label; on_ui(locate); CHECK(found, "control %s is visible", label); if (found) tap(g_btn_x[0], g_btn_y[0]); quiet(); return found != NULL; }
static bool on_page(const char *name) { return !strcmp(ink_ui_page_name(), name); }
static void net(void *link) { sim_net_set_link(link); }
static void set_net(const char *link) { vtk_event_at(vtk_now() + SIM_MS(1), net, (void *)link); delay_ms(5); }
static void cut(void *unused) { (void)unused; sim_storage_power_cut(); }

/* BG-only file service calls, from the test task */
static ink_exec_job_t bg_job;
static volatile int bg_done;
static int indexed, indexed_type;
static uint32_t indexed_id;
static char indexed_path[INK_FS_PATH_MAX];
static void bg_index(void *arg) {
    (void)arg; uint32_t cursor = 0; ink_fs_entry_t e; indexed = 0;
    while (ink_fs_index(VOICE_DIR, INK_FS_ANY, &cursor, &e) == 1) { indexed++; indexed_id = e.id; indexed_type = e.type; snprintf(indexed_path, sizeof indexed_path, "%s", e.path); }
    __atomic_store_n(&bg_done, 1, __ATOMIC_RELEASE);
}
static int index_count(void) { bg_done = 0; ink_exec_post(INK_EXEC_BG, &bg_job, bg_index, NULL); CHECK(WAIT_FOR(bg_done, 10000), "index query"); return indexed; }

static memo_t note;
static int voice_count(void) { /* voice memos of the current member, not deleted */
    ink_db_t *db = ink_db_open_member(&MEMOS, ink_member_current()->id); int count = 0;
    ink_db_cursor_t cur = {0}; ink_rec_t rec;
    while (db && ink_db_next(db, &cur, &rec, NULL, 0) == 1) if (read_note(db, rec.id, &note) && note.path[0] && !note.deleted) count++;
    ink_db_close(db); return count;
}
static bool voice_note(uint32_t id) { ink_db_t *db = ink_db_open_member(&MEMOS, ink_member_current()->id); bool ok = db && read_note(db, id, &note); ink_db_close(db); return ok; }
static uint32_t file_ms(const char *path) {
    ogg_reader_t *r = NULL; int err = 0; uint32_t ms = 0;
    if (ogg_open_for(ink_member_current()->id, path, &r, &err) == OGG_OK) { ms = ogg_duration_ms(r); ogg_close(r); }
    return ms;
}
static void open_list(void) {
    CHECK(BY_HAND("app.open", "{\"id\":\"memo\"}") && g_rc.status == INK_RC_DONE, "open memo");
    for (int i = 0; i < 4 && !on_page("Memo"); i++) { BY_HAND("shell.back", NULL); quiet(); } /* the app comes back where it was */
    CHECK(WAIT_FOR(M.list != NULL && on_page("Memo"), 15000), "memo list loads (%s)", ink_ui_page_name()); quiet();
}
static bool start_recording(void) {
    CHECK(BY_HAND("memo.record", NULL) && g_rc.status == INK_RC_DONE, "memo.record starts the microphone (%s %.100s)", ink_rc_name(g_rc.status), g_rc.say);
    return WAIT_FOR(ink_audio_recording() && on_page("MemoRecord"), 5000);
}
static void press_ai(bool down) { hw(down ? HW_KEY_DOWN : HW_KEY_UP, HAL_KEY_AI, 0); delay_ms(300); }

static void main_phase(void) {
    sim_hub_asr_script("明天记得带作业本|放学去取快递|再问问家长会几点开始", NULL, 0);
    set_net("none"); CHECK(WAIT_FOR(!ink_hub_online(), 30000), "offline first: the words wait"); open_list();
    /* 1. ten seconds, saved: a file on the card, in the index as audio, the length right */
    CHECK(touch("说一句") && WAIT_FOR(ink_audio_recording() && on_page("MemoRecord"), 5000), "说一句 opens MemoRecord with the microphone on");
    snap("MemoRecord");
    CHECK(strstr(snapshot_json, "audio.record_stop") && strstr(snapshot_json, "audio.record_cancel") && strstr(snapshot_json, "不到 1 分钟"),
          "MemoRecord tells the AI how long and both controls (%.150s)", snapshot_json);
    delay_ms(10000);
    CHECK(touch("记好了") && WAIT_FOR(on_page("MemoPending"), 10000), "记好了 saves and shows MemoPending (%s)", ink_ui_page_name());
    CHECK(voice_count() == 1 && voice_note(M.id) && note.pending && note.audio && !strncmp(note.path, VOICE_DIR "/", 10), "one pending voice memo with its file number");
    CHECK(note.ms >= 9500 && note.ms <= 12500, "recorded length about 10 s (%u ms)", (unsigned)note.ms);
    uint32_t first = M.id, ms = file_ms(note.path), audio = note.audio;
    CHECK(ms >= 9500 && ms <= 12500, "the Ogg file on the card is 10 s long (%u ms)", (unsigned)ms);
    CHECK(index_count() == 1 && indexed_type == INK_FS_AUDIO && indexed_id == audio, "the file index has it as audio, the same number (%d %d)", indexed, indexed_type);
    snap("MemoPending");
    CHECK(strstr(snapshot_json, "MemoPending") && strstr(snapshot_json, "memo.play") && strstr(snapshot_json, "连上网后整理成文字"), "MemoPending snapshot (%.150s)", snapshot_json);
    /* 2. cancelled: no file, no memo */
    CHECK(BY_HAND("shell.back", NULL), "back"); open_list();
    CHECK(start_recording(), "recording again"); delay_ms(3000);
    CHECK(touch("不要了") && WAIT_FOR(!ink_audio_recording() && on_page("Memo"), 10000), "不要了 ends it and goes back (%s)", ink_ui_page_name());
    delay_ms(500);
    CHECK(index_count() == 1 && voice_count() == 1, "nothing of the cancelled one stays (%d files, %d memos)", indexed, voice_count());
    /* 3. the AI key while recording: it says so, one tap stops the recording */
    CHECK(start_recording(), "recording for the AI key");
    press_ai(true); press_ai(false); quiet();
    on_ui(take_snapshot);
    CHECK(ink_audio_recording() && strstr(snapshot_json, "正在录音") && strstr(snapshot_json, "audio.record_stop"), "the AI sheet says recording, with a stop (%.150s)", snapshot_json);
    CHECK(touch("停止录音") && WAIT_FOR(!ink_audio_recording(), 5000), "停止录音 on the AI sheet stops it");
    CHECK(WAIT_FOR(voice_count() == 2, 10000), "and it is kept as a memo");
    key(HAL_KEY_HOME); quiet();
    /* 4. online: the hub makes words of them; the recording still plays */
    set_net("wifi");
    CHECK(WAIT_FOR(voice_note(first) && !note.pending, 60000), "the words come back (%d)", note.pending);
    CHECK(strstr(note.body, "明天记得带作业本") != NULL, "the memo has the words (%.100s)", note.body);
    CHECK(WAIT_FOR(voice_count() == 2 && voice_note(first + 1) && !note.pending, 60000), "the second one too");
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)first);
    open_list(); snap("Memo");
    CHECK(strstr(snapshot_json, "明天记得带作业本") && strstr(snapshot_json, "memo.record"), "the list shows the words and 说一句 (%.150s)", snapshot_json);
    CHECK(BY_HAND("memo.open", args) && WAIT_FOR(on_page("MemoDetail"), 5000), "a memo with words opens MemoDetail");
    snap("MemoDetail");
    CHECK(strstr(snapshot_json, "明天记得带作业本") && strstr(snapshot_json, "memo.play"), "MemoDetail shows the words and the player (%.150s)", snapshot_json);
    CHECK(BY_HAND("memo.play", args) && g_rc.status == INK_RC_DONE, "memo.play (%s %.80s)", ink_rc_name(g_rc.status), g_rc.detail);
    ink_media_status_t st; ink_media_status(&st);
    CHECK(st.state == INK_MEDIA_PLAYING && st.duration_ms >= 9500, "the original recording plays (%d %u)", st.state, (unsigned)st.duration_ms);
    CHECK(BY_HAND("media.stop", NULL), "stop playing");
    /* 5. the AI listening: no recording starts, it says why */
    key(HAL_KEY_HOME); quiet(); press_ai(true);
    CHECK(WAIT_FOR(ink_audio_capture_busy(), 3000), "the AI listens");
    CHECK(BY_HAND("memo.record", NULL) && g_rc.status == INK_RC_FAILED && strstr(g_rc.say, "AI"), "no recording while the AI listens (%s %.100s)", ink_rc_name(g_rc.status), g_rc.say);
    press_ai(false); CHECK(!ink_audio_recording(), "none started"); key(HAL_KEY_HOME); quiet();
    /* 6. a call comes: what was recorded is kept first */
    open_list(); CHECK(start_recording(), "recording before the call"); delay_ms(4000);
    snprintf(g_hw.num, sizeof g_hw.num, "13800000000"); hw(HW_CALL, 0, 0);
    CHECK(WAIT_FOR(!ink_audio_recording(), 10000), "the call takes the microphone");
    CHECK(voice_count() == 2, "during the call the memo waits (%d)", voice_count());
    hw(HW_HANGUP, 0, 0); CHECK(WAIT_FOR(strcmp(active_state(), "call"), 10000), "the call ends");
    CHECK(WAIT_FOR(voice_count() == 3, 10000), "the part before the call is a memo");
    uint32_t calls = 0; for (uint32_t id = 1; id < 10; id++) if (voice_note(id) && note.path[0] && note.ms >= 3500 && note.ms <= 5500) calls = id;
    voice_note(calls); CHECK(calls && file_ms(note.path) >= 3500, "about 4 s kept and readable (%u)", (unsigned)calls);
    /* 7. another member sees none of it */
    CHECK(ink_member_switch("kid") == 0, "the child"); quiet();
    CHECK(voice_count() == 0 && index_count() == 0, "no memo and no recording of the adult's");
    CHECK(AI("memo.play", args) && g_rc.status == INK_RC_INVALID, "the adult's recording does not play for the child");
    CHECK(ink_member_switch("owner") == 0, "back to the adult"); quiet();
    CHECK(voice_count() == 3, "the adult's are all there");
}
static void cut_phase(void) {
    open_list(); CHECK(start_recording(), "recording before the power cut"); delay_ms(12000);
    ink_probe_count("test.finished"); vtk_event_at(vtk_now() + SIM_MS(1), cut, NULL); delay_ms(1000);
}
static void after_phase(void) {
    open_list();
    CHECK(WAIT_FOR(voice_count() == 1, 15000), "the recording cut off becomes a memo (%d)", voice_count());
    voice_note(1); uint32_t ms = file_ms(note.path);
    CHECK(note.audio && ms >= 7000 && ms <= 13000, "what was on the card before the cut plays as a good Ogg file (%u ms)", (unsigned)ms);
    CHECK(BY_HAND("memo.play", "{\"id\":1}") && g_rc.status == INK_RC_DONE, "and plays");
}
static void cost_phase(void) { /* D-112: a minute of recording, what it costs the processor, the memory and the card */
    open_list(); sim_metric_add("test.cost_start_ms", (double)(osal_now_us() / 1000));
    CHECK(start_recording(), "a minute of recording"); delay_ms(60000);
    CHECK(BY_HAND("audio.record_stop", NULL) && WAIT_FOR(voice_count() == 1, 10000), "kept");
    sim_metric_add("test.cost_end_ms", (double)(osal_now_us() / 1000));
    voice_note(1); ink_fs_stat_t st = {0}; ink_fs_stat(note.path, &st);
    sim_metric_add("test.cost_file_bytes", (double)st.size); sim_metric_add("test.cost_ms", note.ms);
}
static void run(void *arg) {
    (void)arg; vtk_host_call(host_phase, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    if (!strcmp(phase, "cut")) { cut_phase(); return; }
    if (!strcmp(phase, "after")) after_phase(); else if (!strcmp(phase, "cost")) cost_phase(); else main_phase();
    CHECK(WAIT_FOR(ink_db_idle(), 15000), "writes finish"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "memo-record-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
