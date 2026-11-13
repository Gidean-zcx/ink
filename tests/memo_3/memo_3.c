/* memo-3 on the whole product: the reference walk (APPS.md A09) by touch and by the AI, the three autosave moments,
 * saving while the panel is busy, members, schema-1 records, the copied ZSWatch screen and the refresh of each action.
 * INK_MEMO3_PHASE picks the run (tests/memo_3/run.py); INK_MEMO3_TEXT the text size; INK_MEMO3_KID=1 runs as the child. */
#include "ink/sys/data.h"
static bool refuse_write;
static int test_put(ink_db_t *db, const ink_db_write_t *w, uint32_t *id) { return refuse_write ? INK_DB_ERR_FULL : ink_db_put(db, w, id); }
#define ink_db_put test_put
#ifndef MEMO3_SOURCE
#define MEMO3_SOURCE "../../system/apps/memo/memo.c"
#endif
#include MEMO3_SOURCE
#undef ink_db_put
#ifndef MEMO3_VENDOR
#define MEMO3_VENDOR "../../system/apps/memo/vendor/zswatch/voice_memo_ui.c"
#endif
#include MEMO3_VENDOR
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include <stdlib.h>

static char phase[24], snap_json[12000], scene[48], label_text[160];
static int text_size = 1;
static bool kid;
static const char *wanted;
static lv_obj_t *seen;
static void host_env(void *unused) {
    (void)unused; const char *p = getenv("INK_MEMO3_PHASE"), *t = getenv("INK_MEMO3_TEXT"), *k = getenv("INK_MEMO3_KID");
    snprintf(phase, sizeof phase, "%s", p ? p : "walk"); text_size = t ? atoi(t) : 1; kid = k && *k == '1';
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "display settles"); }
static bool on_page(const char *name) { return !strcmp(ink_ui_page_name(), name); }
static void take(void) { CHECK(ink_ui_snapshot(snap_json, sizeof snap_json, NULL) > 0, "%s has an AI snapshot", scene); }
static bool captured;
static void capture(void *name) { sim_display_snapshot(name); captured = true; }
static void shot(const char *name) { /* the settled page: snap-<name>.png, and its AI snapshot */
    snprintf(scene, sizeof scene, "%s", name); quiet(); on_ui(take); captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), capture, scene); CHECK(WAIT_FOR(captured, 1000), "captured %s", name);
}
static void look(lv_obj_t *obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), wanted)) seen = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) look(lv_obj_get_child(obj, i));
}
static void locate(void) {
    seen = NULL; look(lv_screen_active()); if (!seen) look(lv_layer_top()); if (!seen) look(lv_layer_sys());
    if (!seen) return;
    lv_obj_t *b = lv_obj_get_parent(seen); lv_area_t r; lv_obj_update_layout(b); lv_obj_get_coords(b, &r);
    CHECK(lv_area_get_height(&r) >= ink_ui_px(INK_CONTROL), "%s is a full touch target", wanted);
    g_btn_x[0] = (r.x1 + r.x2) / 2; g_btn_y[0] = (r.y1 + r.y2) / 2;
}
static bool visible(const char *text) { wanted = text; on_ui(locate); return seen != NULL; }
static bool touch(const char *text) { bool ok = visible(text); CHECK(ok, "%s is on screen (%s)", text, ink_ui_page_name()); if (ok) tap(g_btn_x[0], g_btn_y[0]); quiet(); return ok; }
static void ok(const char *cap, const char *args, ink_source_t src) {
    CHECK(call_as(cap, args, src) && g_rc.status == INK_RC_DONE, "%s %.80s (%s %.80s)", cap, args ? args : "", ink_rc_name(g_rc.status), g_rc.say);
    quiet();
}
static memo_t note;
static bool load(const char *member, uint32_t id) {
    ink_db_t *db = ink_db_open_member(&MEMOS, member); bool found = db && read_note(db, id, &note); ink_db_close(db); return found;
}
static const char *typing;
static void type_now(void) { ink_text_commit_utf8(typing); }
static void type(const char *text) { typing = text; on_ui(type_now); }
static void enter_now(void) { ink_text_enter(); }
static void blur_now(void) { ink_text_blur(0); }
static void status_now(void) { snprintf(label_text, sizeof label_text, "%s", M.notice ? lv_label_get_text(M.notice) : ""); }
static const char *status(void) { on_ui(status_now); return label_text; }
static void mark(const char *what, bool after) { char name[64]; snprintf(name, sizeof name, "memo3.%s.%s", what, after ? "after" : "before"); quiet(); ink_probe_mark(name); }
static void open_list(void) {
    CHECK(BY_HAND("app.open", "{\"id\":\"memo\"}") && g_rc.status == INK_RC_DONE, "open memo");
    for (int i = 0; i < 5 && !on_page("Memo"); i++) { BY_HAND("shell.back", NULL); quiet(); }
    CHECK(WAIT_FOR(M.list && on_page("Memo"), 15000), "the memo list (%s)", ink_ui_page_name()); quiet();
}
static uint32_t newest(void) { /* the id of the newest change */
    ink_db_t *db = ink_db_open_member(&MEMOS, ink_member_current()->id); ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec = {0};
    uint32_t id = db && ink_db_next(db, &cur, &rec, NULL, 0) == 1 ? rec.id : 0; ink_db_close(db); return id;
}
static bool reminders(const char *title) { return call_as("reminder.list", NULL, INK_SRC_AI) && strstr(g_result, title) != NULL; }

/* ---------------------------------------------------------------- the reference walk */
static char long_text[4096];
static void walk(ink_source_t src) {
    bool ai = src == INK_SRC_AI; char args[256]; const char *me = ink_member_current()->id;
    open_list(); shot(ai ? "Memo-empty-ai" : "Memo-empty");
    CHECK(strstr(snap_json, "还没有备忘") && strstr(snap_json, "memo.search") && strstr(snap_json, "memo.record"), "the empty list says so and offers its actions");
    /* 1. a long memo, up to the editor's limit (4095 bytes: 1365 characters) */
    if (ai) ok("memo.edit", NULL, src); else touch("写一条");
    CHECK(on_page("MemoEdit") && M.field && ink_text_focused(), "写一条 opens the editor with the keyboard up");
    uint32_t long_id = M.id; size_t n = 0;
    while (n + 3 < 4095) { memcpy(long_text + n, "记", 3); n += 3; }
    long_text[n] = 0; type(long_text); quiet();
    CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000) && load(me, long_id) && strlen(note.body) == n, "1365 characters are written (%zu bytes)", strlen(note.body));
    shot(ai ? "MemoEdit-long-ai" : "MemoEdit-long");
    mark("type", false); type("。"); quiet(); mark("type", true); /* the last character that fits: 4095 bytes */
    n += 3; CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000) && load(me, long_id) && strlen(note.body) == n, "4095 bytes, 1365 characters");
    /* 2. another app and back: everything is there, no save was pressed */
    ok("app.open", "{\"id\":\"calculator\"}", src); open_list();
    CHECK(load(me, long_id) && strlen(note.body) == n, "after another app the memo holds every character");
    /* a memo with three hits, then the search */
    ok("memo.edit", NULL, src); uint32_t hit_id = M.id;
    type("明天带作业本\n放学前看看作业本在不在\n别忘了作业本和水杯"); quiet(); ok("system.home", NULL, src); open_list();
    ok("system.home", NULL, src); mark("open_list", false); open_list(); mark("open_list", true);
    mark("search", false); ok("memo.search", "{\"query\":\"作业本\"}", ai ? src : INK_SRC_TOUCH); mark("search", true);
    shot(ai ? "Memo-search-ai" : "Memo-search");
    CHECK(strstr(snap_json, "共 3 处") && strstr(snap_json, "\"search\":\"作业本\""), "the result row says 共 3 处 (%.200s)", snap_json);
    snprintf(args, sizeof args, "{\"id\":%u,\"query\":\"作业本\"}", (unsigned)hit_id);
    mark("open_memo", false);
    if (ai) ok("memo.open", args, src); else touch("明天带作业本");
    mark("open_memo", true); CHECK(WAIT_FOR(F.count == 3, 5000), "the find bar counts 3");
    shot(ai ? "MemoEdit-find-ai" : "MemoEdit-find");
    CHECK(on_page("MemoEdit") && !strcmp(status(), "第 1 处，共 3 处"), "the first hit, 第 1 处，共 3 处 (%s)", label_text);
    CHECK(strstr(snap_json, "\\\"at\\\":1,\\\"count\\\":3") || strstr(snap_json, "\"at\":1,\"count\":3"), "the AI is told where (%.200s)", strstr(snap_json, "\"find\"") ? strstr(snap_json, "\"find\"") : snap_json);
    size_t start = 0, end = 0; ink_text_selection(&start, &end);
    CHECK(end - start == 3 && start == 3, "the hit is selected in the body (%zu..%zu)", start, end);
    mark("find_next", false);
    if (ai) ok("memo.find_next", NULL, src); else touch("下一个");
    mark("find_next", true);
    CHECK(!strcmp(status(), "第 2 处，共 3 处"), "下一个: 第 2 处，共 3 处 (%s)", label_text);
    ok("memo.find_next", NULL, src); ok("memo.find_next", NULL, src); CHECK(!strcmp(status(), "第 1 处，共 3 处"), "after the last it wraps to the first");
    snprintf(args, sizeof args, "{\"id\":%u,\"query\":\"没有这个词\"}", (unsigned)hit_id); ok("memo.find", args, src);
    shot(ai ? "MemoEdit-find-none-ai" : "MemoEdit-find-none"); CHECK(!strcmp(status(), "没有找到"), "没有找到 (%s)", label_text);
    /* This device has no back key (the profile's keys); 关闭 in the top bar closes the find bar and the page stays */
    if (!ai) touch("关闭"); else ok("memo.find_close", NULL, src);
    CHECK(on_page("MemoEdit") && !F.open && visible("查找"), "关闭: the find bar goes, the editor's own buttons come back");
    ok("system.home", NULL, src); open_list();
    ok("memo.search", "{\"query\":\"不存在的字\"}", ai ? src : INK_SRC_TOUCH); shot(ai ? "Memo-search-none-ai" : "Memo-search-none");
    CHECK(visible("没有找到") && visible("清除搜索"), "no result: the shared empty state and 清除搜索");
    if (ai) ok("memo.search", "{\"query\":\"\"}", src); else touch("清除搜索");
    CHECK(WAIT_FOR(!S.on, 3000), "the search is cleared");
    /* 3. a checklist: five items, two checked go under 已完成 2 项, the list says 2/5 已完成 */
    const char *items[] = {"牛奶", "电池", "面包", "鸡蛋", "抹布"};
    if (ai) ok("memo.edit", "{\"checklist\":true}", src); else touch("新清单");
    CHECK(on_page("MemoChecklist"), "新清单 opens an empty checklist (%s)", ink_ui_page_name());
    uint32_t list_id = M.id;
    shot(ai ? "MemoChecklist-empty-ai" : "MemoChecklist-empty");
    if (ai) { snprintf(args, sizeof args, "{\"id\":%u,\"text\":\"牛奶\\n电池\\n面包\\n鸡蛋\\n抹布\"}", (unsigned)list_id); ok("memo.add_item", args, src); }
    else for (int i = 0; i < 5; i++) {
        if (i == 4) mark("add_item", false);
        type(items[i]); on_ui(enter_now); CHECK(WAIT_FOR(load(me, list_id) && lines(note.body) == (unsigned)i + 1, 5000), "item %d is added", i + 1); quiet();
        if (i == 4) mark("add_item", true);
        if (i == 1) shot("MemoChecklist-add");
    }
    CHECK(load(me, list_id) && note.checklist && !strcmp(note.body, "牛奶\n电池\n面包\n鸡蛋\n抹布"), "five items, in order (%.60s)", note.body);
    if (!ai) touch("完成");
    for (int i = 0; i < 2; i++) {
        if (i == 1) mark("check", false);
        if (ai) { snprintf(args, sizeof args, "{\"id\":%u,\"index\":%d}", (unsigned)list_id, i); ok("memo.check", args, src); }
        else touch(items[i]);
        if (i == 1) mark("check", true);
    }
    CHECK(load(me, list_id) && note.checked == 3, "the first two are checked (%u)", (unsigned)note.checked);
    shot(ai ? "MemoChecklist-done-collapsed-ai" : "MemoChecklist-done-collapsed");
    if (!visible("已完成 2 项 · 展开")) { ok("memo.next", NULL, src); on_ui(take); } /* the largest text: fewer rows a page */
    CHECK(strstr(snap_json, "已完成 2 项") && visible("已完成 2 项 · 展开"), "the done ones are under 已完成 2 项");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)list_id);
    if (ai) ok("memo.show_done", args, src); else touch("已完成 2 项 · 展开");
    shot(ai ? "MemoChecklist-done-expanded-ai" : "MemoChecklist-done-expanded");
    CHECK(visible("删掉已完成") && visible("全部取消勾选"), "expanded: 删掉已完成 and 全部取消勾选");
    ok("system.home", NULL, src); open_list(); shot(ai ? "Memo-list-ai" : "Memo-list");
    CHECK(strstr(snap_json, "2/5 已完成"), "the list row says 2/5 已完成");
    /* 4. tomorrow 9:00 for an item: reminder.list has it; checking the item takes it away */
    snprintf(args, sizeof args, "{\"id\":%u,\"index\":2}", (unsigned)list_id); ok("memo.open", args, src);
    CHECK(on_page("MemoItem"), "the item page (%s)", ink_ui_page_name()); shot(ai ? "MemoItem-ai" : "MemoItem");
    if (!ai) { touch("设到期时间"); CHECK(on_page("MemoDue"), "the due page"); shot("MemoDue"); }
    ink_tm_t t; ink_time_to_local(ink_time_now_s() + 86400, &t);
    char at[24]; snprintf(at, sizeof at, "%04d-%02d-%02d 09:00", t.year % 10000, t.month % 100, t.day % 100);
    mark("set_due", false);
    if (ai) { snprintf(args, sizeof args, "{\"id\":%u,\"index\":2,\"at\":\"%s\"}", (unsigned)list_id, at); ok("memo.set_due", args, src); }
    else touch("设好了");
    mark("set_due", true);
    CHECK(WAIT_FOR(load(me, list_id) && note.due[2] && note.rid[2], 5000), "the item has its due date and a reminder (%u)", (unsigned)note.rid[2]);
    CHECK(reminders("面包"), "reminder.list has 面包 (%.200s)", g_result);
    snprintf(args, sizeof args, "{\"id\":%u,\"index\":2}", (unsigned)list_id); ok("memo.check", args, src);
    CHECK(load(me, list_id) && !note.rid[2] && !reminders("面包"), "checked: the reminder is gone");
    /* 5. a recording, made into words by the hub's stand-in */
    if (!ai) {
        ok("system.home", NULL, src); open_list();
        mark("start_record", false); touch("说一句"); mark("start_record", true);
        CHECK(on_page("MemoRecord") && ink_audio_recording(), "说一句 records"); shot("MemoRecord");
        mark("minute", false); delay_ms(62000); mark("minute", true);
        on_ui(take); CHECK(strstr(snap_json, "1 分钟"), "a minute later the time line says 1 分钟 (%.120s)", snap_json);
        mark("stop", false); touch("记好了"); mark("stop", true);
        CHECK(!call_as("memo.record", NULL, INK_SRC_TOUCH) || g_rc.status == INK_RC_STALE, "a second tap within 2 s starts nothing (%s)", ink_rc_name(g_rc.status));
        CHECK(WAIT_FOR(on_page("MemoPending"), 10000), "记好了 saves into MemoPending (%s)", ink_ui_page_name()); shot("MemoPending");
        CHECK(strstr(snap_json, "录于") && strstr(snap_json, "连上网后整理成文字"), "MemoPending says when it was recorded");
        uint32_t voice_id = M.id;
        sim_hub_asr_script("记得给奶奶打电话", NULL, 0);
        CHECK(WAIT_FOR(load(me, voice_id) && !note.pending && strstr(note.body, "奶奶"), 60000), "the hub's words go into this memo (%.60s)", note.body);
        snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)voice_id); ok("memo.open", args, src);
        shot("MemoDetail-voice"); char play[48]; snprintf(play, sizeof play, "播放录音 · %u:%02u", (unsigned)(note.ms / 60000), (unsigned)(note.ms / 1000 % 60));
        CHECK(on_page("MemoDetail") && visible(play) && strstr(snap_json, "memo.play"), "the voice memo plays from a written button (%s)", play);
    } else {
        CHECK(call_as("memo.record", NULL, src) && (g_rc.status == INK_RC_PENDING_CONFIRMATION || g_rc.status == INK_RC_DONE), "the AI's 说一句 goes through the microphone rule (%s)", ink_rc_name(g_rc.status));
        if (g_rc.status == INK_RC_DONE) { ok("audio.record_stop", NULL, src); CHECK(WAIT_FOR(on_page("MemoPending"), 10000), "stopped by the AI it is kept"); }
        else { quiet(); touch("取消"); } /* the microphone card waits for the person: here they say no */
    }
    /* 6. a failed write: the words stay, the draft is on the device, 重试 stores them */
    ok("system.home", NULL, src); open_list(); ok("memo.edit", NULL, src);
    refuse_write = true; mark("save_fail", false);
    type("存不上也不能丢的字"); CHECK(WAIT_FOR(M.mode == FAIL, 15000), "the failure view"); mark("save_fail", true);
    CHECK(WAIT_FOR(ink_text_draft_saved() == 1, 15000), "the draft is on the device");
    shot(ai ? "MemoSaveFail-ai" : "MemoSaveFail");
    CHECK(!strcmp(status(), "草稿已存，备忘没存上，请重试") && !strcmp(ink_textfield_text(M.field), "存不上也不能丢的字"), "草稿已存，备忘没存上，请重试 and every character (%s)", label_text);
    refuse_write = false; uint32_t fail_id = M.id;
    if (ai) ok("memo.retry", NULL, src); else touch("重试");
    CHECK(WAIT_FOR(!M.saving && ink_db_idle() && load(me, fail_id) && !strcmp(note.body, "存不上也不能丢的字"), 15000), "重试 stores them");
    ok("system.home", NULL, src);
}

/* ---------------------------------------------------------------- autosave in the same UI turn as the last key */
static uint32_t edit_id;
static void key_then_leave(void) { ink_text_commit_utf8("后"); ink_ui_pop(); }
static void key_then_member(void) { ink_text_commit_utf8("后"); ink_member_switch("kid"); ink_ui_home(); }
static void key_then_lock(void) { ink_text_commit_utf8("后"); ink_power_lock(); }
static void start_edit(void) {
    open_list(); ok("memo.edit", NULL, INK_SRC_TOUCH); edit_id = M.id; type("先"); CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000), "the first word is stored");
}
static void autosave(void) {
    start_edit();
    if (!strcmp(phase, "leave")) on_ui(key_then_leave);
    else if (!strcmp(phase, "member")) on_ui(key_then_member);
    else { on_ui(key_then_lock); CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 5000), "locked (%s)", power_state()); delay_ms(500); }
    CHECK(load("owner", edit_id) && !strcmp(note.body, "先后"), "%s: the last key is in the memo at once (%s)", phase, note.body);
    if (!strcmp(phase, "lock")) CHECK(M.field && !strcmp(ink_ui_page_name(), "MemoEdit"), "the lock covers the editor, the page stays (%s)", ink_ui_page_name());
    CHECK(WAIT_FOR(ink_db_idle(), 15000), "and becomes durable");
}

/* ---------------------------------------------------------------- saving while typing keeps the panel busy */
static lv_timer_t *typer;
static unsigned typed;
static char save_args[160], final_text[512];
static void tick(lv_timer_t *t) { (void)t; ink_receipt_t rc; ink_cap_invoke_json("keyboard.key", "{\"code\":120}", INK_SRC_TOUCH, ink_ui_content_version(), &rc); typed++; }
static void typing_on(void) { if (!ink_text_english()) ink_text_key(INK_TEXT_KEY_LANGUAGE); snprintf(save_args, sizeof save_args, "{\"id\":%u,\"draft\":%u,\"page\":\"%s\"}", (unsigned)M.id, (unsigned)M.note.draft, M.note.page); typer = lv_timer_create(tick, 50, NULL); }
static void typing_off(void) { lv_timer_delete(typer); snprintf(final_text, sizeof final_text, "%s", ink_textfield_text(M.field)); }
static void busy(void) {
    open_list(); ok("memo.edit", NULL, INK_SRC_TOUCH); uint32_t id = M.id; ink_power_hold(INK_NEED_AWAKE);
    on_ui(typing_on); delay_ms(800);
    bool answered = BY_HAND("memo.save", save_args);
    CHECK(answered && (g_rc.status == INK_RC_DONE || g_rc.status == INK_RC_FAILED), "the save answers while typing goes on (%s)", ink_rc_name(g_rc.status));
    delay_ms(4000); on_ui(typing_off); quiet();
    CHECK(strlen(final_text) == typed && typed > 50, "every key is in the editor (%zu of %u)", strlen(final_text), typed);
    /* The brief wants the draft on the device here. The text service does not write a refused draft again until the next key
     * (text_service.c save_draft): recorded, and the status line must then say so instead of claiming it. */
    bool draft = WAIT_FOR(ink_text_draft_saved() == 1, 60000);
    sim_metric_set("memo3.busy_draft_durable", draft ? 1 : 0);
    CHECK(draft || !strcmp(status(), "草稿没存上，字还在，请重试"), "the draft is on the device, or the page says it is not (%d %s)", ink_text_draft_saved(), label_text);
    if (M.mode == FAIL) touch("重试");
    CHECK(WAIT_FOR(!M.saving && ink_db_idle() && load("owner", id) && !strcmp(note.body, final_text), 15000), "the memo ends up with exactly what was typed (%zu vs %zu, mode %d)", strlen(note.body), strlen(final_text), M.mode);
}

/* ---------------------------------------------------------------- members */
static void isolation(void) {
    char args[96]; open_list(); ok("memo.edit", NULL, INK_SRC_TOUCH); uint32_t id = M.id; type("家长的秘密清单"); quiet();
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id); ok("system.home", NULL, INK_SRC_TOUCH);
    ink_tm_t t; ink_time_to_local(ink_time_now_s() + 86400, &t);
    snprintf(args, sizeof args, "{\"id\":%u,\"at\":\"%04d-%02d-%02d 10:00\"}", (unsigned)id, t.year % 10000, t.month % 100, t.day % 100); ok("memo.set_due", args, INK_SRC_AI);
    CHECK(reminders("家长的秘密清单"), "the adult's reminder");
    ok("memo.search", "{\"query\":\"秘密\"}", INK_SRC_AI); CHECK(strstr(g_result, "家长的秘密清单") != NULL, "the adult finds it");
    CHECK(ink_member_switch("kid") == 0, "the child"); quiet(); open_list();
    ok("memo.search", "{\"query\":\"秘密\"}", INK_SRC_AI); CHECK(!strstr(g_result, "家长") && !S.count, "the child finds nothing of the adult's (%.150s)", g_result);
    CHECK(!reminders("家长的秘密清单"), "and sees no adult reminder");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id); CHECK(AI("memo.open", args) && g_rc.status == INK_RC_INVALID, "nor opens it");
    ok("memo.edit", NULL, INK_SRC_TOUCH); type("孩子的秘密"); quiet(); ok("system.home", NULL, INK_SRC_TOUCH); open_list();
    ok("memo.search", "{\"query\":\"秘密\"}", INK_SRC_AI); CHECK(strstr(g_result, "孩子的秘密") && S.count == 1, "the child finds only its own");
    ok("memo.edit", "{\"checklist\":true}", INK_SRC_TOUCH);
    ink_time_to_local(ink_time_now_s() + 7200, &t);
    snprintf(args, sizeof args, "{\"id\":%u,\"text\":\"交作业\"}", (unsigned)M.id); ok("memo.add_item", args, INK_SRC_AI);
    snprintf(args, sizeof args, "{\"id\":%u,\"index\":0,\"at\":\"%04d-%02d-%02d %02d:%02d\"}", (unsigned)M.id, t.year % 10000, t.month % 100, t.day % 100, t.hour, t.min);
    ok("memo.set_due", args, INK_SRC_TOUCH); CHECK(reminders("交作业"), "a child's due item becomes the child's own reminder");
    CHECK(ink_member_switch("owner") == 0, "back to the adult"); quiet(); CHECK(!reminders("交作业"), "the adult does not see it");
}

/* ---------------------------------------------------------------- schema 1 records */
static bool put_raw(uint32_t id, const char *json) {
    ink_db_t *db = ink_db_open_member(&MEMOS, "owner"); uint32_t ver = 0;
    ink_db_write_t w = {.id = id, .data = json, .len = strlen(json), .schema = 1, .src = INK_SRC_SYSTEM, .written_ver = &ver};
    bool done = db && ink_db_put(db, &w, NULL) == 0 && ink_db_wait(db, ver, 5000000) == 0; ink_db_close(db); return done;
}
static void old_records(void) {
    CHECK(put_raw(7, "{\"body\":\"旧的文本备忘\",\"checklist\":false,\"deleted\":false,\"checked\":0,\"draft\":0,\"page\":\"\"}"), "a schema-1 text memo");
    CHECK(put_raw(8, "{\"body\":\"旧一\\n旧二\\n旧三\",\"checklist\":true,\"deleted\":false,\"checked\":5,\"draft\":0,\"page\":\"\"}"), "a schema-1 checklist");
    CHECK(put_raw(9, "{\"body\":\"旧的录音文字\",\"checklist\":false,\"deleted\":false,\"checked\":0,\"draft\":0,\"page\":\"\",\"path\":\"/me/memos/20261101-080000.opus\",\"audio\":42,\"ms\":65000,\"pending\":false}"), "a schema-1 voice memo");
    CHECK(load("owner", 7) && !strcmp(note.body, "旧的文本备忘") && !note.checklist && !note.due[0], "the old text memo reads whole");
    CHECK(load("owner", 8) && note.checklist && note.checked == 5 && lines(note.body) == 3, "the old checklist keeps its checks");
    CHECK(load("owner", 9) && note.audio == 42 && note.ms == 65000 && !strcmp(note.path, "/me/memos/20261101-080000.opus"), "the old voice memo keeps its recording");
    open_list(); on_ui(take);
    CHECK(strstr(snap_json, "旧的文本备忘") && strstr(snap_json, "2/3 已完成") && strstr(snap_json, "旧的录音文字"), "all three are listed (%.250s)", strstr(snap_json, "memo:8:0") ? strstr(snap_json, "memo:8:0") : snap_json);
    ok("memo.open", "{\"id\":9}", INK_SRC_AI); CHECK(on_page("MemoDetail") && visible("播放录音 · 1:05"), "播放录音 · 1:05");
    ok("search.query", "{\"query\":\"旧的文本\"}", INK_SRC_AI);
    CHECK(strstr(g_result, "旧的文本备忘") != NULL, "the global search still reads memo.notes (%.150s)", g_result);
    ok("memo.check", "{\"id\":8,\"index\":1}", INK_SRC_AI);
    CHECK(load("owner", 8) && note.checked == 7, "an old checklist takes a new check and is written as schema 2");
}

/* ---------------------------------------------------------------- the copied ZSWatch screen, alone */
static lv_obj_t *vendor_root;
static void vendor_states(void) {
    static const voice_memo_ui_callbacks_t none = {0};
    vendor_root = lv_obj_create(lv_screen_active());
    voice_memo_ui_show(vendor_root, &none); voice_memo_ui_show_recording();
    CHECK(ui.rec_screen && !lv_obj_has_flag(ui.rec_screen, LV_OBJ_FLAG_HIDDEN) && !strcmp(lv_label_get_text(ui.rec_indicator), "正在记"), "recording: the still 正在记 label");
    CHECK(!strcmp(lv_label_get_text(ui.time_label), "不到 1 分钟") && !ui.list_screen, "recording: 不到 1 分钟, no list screen");
    lv_opa_t opa = lv_obj_get_style_opa(ui.rec_indicator, 0); bool steady = true;
    for (uint32_t ms = 0; ms < 60000; ms += 200) { voice_memo_ui_update_time(ms, 3600); steady &= lv_obj_get_style_opa(ui.rec_indicator, 0) == opa; }
    CHECK(steady && !strcmp(lv_label_get_text(ui.time_label), "不到 1 分钟"), "300 updates in the first minute: no blinking, the same line");
    voice_memo_ui_update_time(125000, 3600);
    CHECK(!strcmp(lv_label_get_text(ui.time_label), "2 分钟") && !strcmp(lv_label_get_text(ui.remaining_label), "还能录约 60 分钟"), "by the minute: 2 分钟, 还能录约 60 分钟");
    voice_memo_ui_show_saving();
    CHECK(lv_obj_has_flag(ui.stop_btn, LV_OBJ_FLAG_HIDDEN) && !strcmp(lv_label_get_text(ui.remaining_label), "正在保存录音"), "saving: the stop button hidden, 正在保存录音");
    voice_memo_ui_remove();
    CHECK(!ui.rec_screen && !ui.time_label && !ui.stop_btn && !ui.rec_indicator && !ui.remaining_label && !ui.root_obj, "remove: every object let go");
    lv_obj_delete(vendor_root);
}

static void run(void *arg) {
    (void)arg; vtk_host_call(host_env, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    ink_receipt_t rc; CHECK(ink_setting_set("display.font_size", text_size, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "text size %d", text_size);
    sim_hub_asr_script("", NULL, 0);
    if (!strcmp(phase, "walk") || !strcmp(phase, "ai")) {
        if (kid) { CHECK(ink_member_switch("kid") == 0, "as the child"); quiet(); }
        walk(!strcmp(phase, "ai") ? INK_SRC_AI : INK_SRC_TOUCH);
    } else if (!strcmp(phase, "leave") || !strcmp(phase, "lock") || !strcmp(phase, "member")) autosave();
    else if (!strcmp(phase, "busy")) busy();
    else if (!strcmp(phase, "isolation")) isolation();
    else if (!strcmp(phase, "old")) old_records();
    else if (!strcmp(phase, "vendor")) on_ui(vendor_states);
    CHECK(WAIT_FOR(ink_db_idle(), 15000), "writes finish"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "memo3-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the memo-3 driver");
}
