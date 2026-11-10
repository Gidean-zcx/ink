/* Real application pages, shared label geometry checks, and synthetic member data only. */
#include <stdlib.h>
#include "ink/sys/i18n.h"
#include "ink/sys/data.h"
#ifdef INK_I18N_MEMO
static bool reject_note;
static int test_note_put(ink_db_t *db, const ink_db_write_t *w, uint32_t *id) {
    return reject_note ? INK_DB_ERR_FULL : ink_db_put(db, w, id);
}
#define ink_db_put test_note_put
#include "../system/apps/memo/memo.c"
#undef ink_db_put
#else
#include "../system/apps/remote/remote.c"
#endif
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include "ink/sys/camera.h"
#include "ink/sys/file.h"
#include "ink/sys/hub.h"
#include "ink/sys/media.h"

static bool english, frame_done, overlay_mode;
static const char *scene;
static char page_json[16384], ui_labels[16384];
static lv_area_t text_boxes[256];
static unsigned box_count;
static double missing_before;
static bool test_registered(void) { int64_t value = 0; return ink_state_get_int(ink_state_find("cell.registered"), &value) && value; }
static bool test_settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void test_quiet(void) { CHECK(WAIT_FOR(test_settled(), 15000), "page settles: %s", ink_ui_page_name()); }
static void test_ok(const char *cap, const char *args) {
    CHECK(BY_HAND(cap, args) && g_rc.status == INK_RC_DONE, "%s: %s %.100s", cap, ink_rc_name(g_rc.status), g_rc.say);
    test_quiet();
}
static void test_home(void) { test_ok("system.home", NULL); }
static void test_open(const char *app) {
    char args[64]; test_home(); snprintf(args, sizeof args, "{\"id\":\"%s\"}", app); test_ok("app.open", args);
}
static void test_walk(lv_obj_t *o) {
    if (!lv_obj_is_visible(o) || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && lv_label_get_text(o)[0]) {
        const char *t = lv_label_get_text(o);
        const lv_font_t *font = lv_obj_get_style_text_font(o, LV_PART_MAIN);
        int32_t letter = lv_obj_get_style_text_letter_space(o, LV_PART_MAIN), line = lv_obj_get_style_text_line_space(o, LV_PART_MAIN);
        lv_area_t a; lv_obj_get_content_coords(o, &a);
        lv_point_t one, wrapped;
        lv_text_get_size(&one, t, font, letter, line, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_text_get_size(&wrapped, t, font, letter, line, lv_area_get_width(&a), LV_TEXT_FLAG_NONE);
        bool single = lv_label_get_long_mode(o) != LV_LABEL_LONG_MODE_WRAP;
        size_t len = strlen(t);
        CHECK(!(lv_label_get_long_mode(o) == LV_LABEL_LONG_MODE_DOTS && len >= 3 && !strcmp(t + len - 3, "...")),
              "%s: no ellipsis: %.100s", scene, t);
        CHECK(single ? one.x <= lv_area_get_width(&a) + 1 : wrapped.y <= lv_area_get_height(&a) + 1,
              "%s: text fits (%d/%d, %d/%d): %.100s", scene, one.x, lv_area_get_width(&a), wrapped.y, lv_area_get_height(&a), t);
        int32_t width = LV_MIN(single ? one.x : wrapped.x, lv_area_get_width(&a));
        lv_text_align_t align = lv_obj_get_style_text_align(o, LV_PART_MAIN);
        if (align == LV_TEXT_ALIGN_RIGHT) a.x1 = a.x2 - width + 1;
        else if (align == LV_TEXT_ALIGN_CENTER) a.x1 += (lv_area_get_width(&a) - width) / 2;
        a.x2 = a.x1 + width - 1; a.y2 = a.y1 + (single ? one.y : wrapped.y) - 1;
        CHECK(a.x1 >= 0 && a.y1 >= 0 && a.x2 < ink_ui_width() && a.y2 < ink_ui_height(), "%s: on screen: %.100s", scene, t);
        for (unsigned i = 0; i < box_count; i++) {
            lv_area_t b = text_boxes[i];
            CHECK(a.x1 > b.x2 || b.x1 > a.x2 || a.y1 > b.y2 || b.y1 > a.y2, "%s: no overlapping text: %.100s", scene, t);
        }
        CHECK(box_count < sizeof text_boxes / sizeof *text_boxes, "label geometry buffer is large enough");
        if (box_count < sizeof text_boxes / sizeof *text_boxes) text_boxes[box_count++] = a;
        size_t at = strlen(ui_labels);
        int n = snprintf(ui_labels + at, sizeof ui_labels - at, "%s\n", t);
        CHECK(n >= 0 && (size_t)n < sizeof ui_labels - at, "label evidence fits");
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) test_walk(lv_obj_get_child(o, (int32_t)i));
}
static void test_save(void *arg) {
    (void)arg; char path[1024];
    int n = snprintf(path, sizeof path, "%s/pages.ndjson", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    size_t cap = 6 * strlen(page_json) + 3; char *quoted = malloc(cap);
    if (!quoted) abort();
    ink_jw_t w; ink_jw_init(&w, quoted, cap); ink_jw_str(&w, page_json);
    FILE *f = fopen(path, "ab");
    if (w.overflow || !f || fprintf(f, "{\"scene\":\"%s\",\"snapshot\":%s}\n", scene, quoted) < 0 || fclose(f)) abort();
    free(quoted);
}
static void test_read(void) {
    box_count = 0; ui_labels[0] = 0;
    CHECK((overlay_mode ? ink_ui_context_snapshot(page_json, sizeof page_json, NULL) : ink_ui_snapshot(page_json, sizeof page_json, NULL)) > 0, "%s exports its snapshot", scene);
    lv_obj_update_layout(lv_screen_active()); lv_obj_update_layout(lv_layer_sys());
    if (overlay_mode) test_walk(lv_obj_get_child(lv_layer_sys(), -1));
    else { test_walk(lv_screen_active()); test_walk(lv_layer_sys()); }
    vtk_host_call(test_save, NULL);
}
static void test_frame(void *arg) {
    (void)arg; CHECK(test_settled(), "%s is presented", scene); sim_display_snapshot(scene); frame_done = true;
}
static void test_capture(const char *name, const char *zh, const char *en) {
    scene = name; test_quiet(); on_ui(test_read);
    CHECK(strstr(page_json, english ? en : zh) || strstr(ui_labels, english ? en : zh),
          "%s has its %s text %s: %.100s", name, english ? "English" : "Chinese", english ? en : zh, page_json);
    frame_done = false; vtk_event_at(vtk_now() + SIM_MS(1), test_frame, NULL);
    CHECK(WAIT_FOR(frame_done, 2000), "%s screenshot saved", name);
    CHECK(!english || sim_metric_value("i18n.missing") == missing_before, "%s has no missing translations (%g)", name,
          sim_metric_value("i18n.missing") - missing_before);
    ink_probe_count("test.frames");
}
static uint32_t test_fixture(const char *collection, const char *json) {
    ink_db_t *db = ink_db_open_member(ink_db_find_def(collection), "owner"); uint32_t id = 0;
    ink_db_write_t w = {.data = json, .len = strlen(json), .src = INK_SRC_SYSTEM};
    CHECK(db && ink_db_put(db, &w, &id) == INK_DB_OK, "synthetic fixture for %s", collection);
    if (db) { CHECK(ink_db_wait(db, ink_db_last_ver(db), 10000000) == 0, "fixture durable"); ink_db_close(db); }
    return id;
}
static void test_mode(void *arg) { (void)arg; const char *lang = getenv("INK_I18N_APPS_LANG"); english = lang && !strcmp(lang, "en"); }

#ifdef INK_I18N_MEMO
static void test_memos(void) {
    test_open("memo"); CHECK(WAIT_FOR(M.list != NULL, 15000), "memo collection loaded");
    test_capture("Memo-empty", "还没有备忘", "No notes yet");
    test_ok("memo.edit", NULL); test_capture("MemoEdit", "备忘", "Notes");
    test_ok("text.insert", "{\"text\":\"用户原文 晴\\nmilk\"}");
    CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000), "memo saved");
    uint32_t id = M.id; char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", id);
    test_ok("text.submit", NULL); test_ok("memo.open", args);
    test_capture("MemoDetail", "编辑备忘", "Edit"); CHECK(strstr(page_json, "用户原文 晴"), "memo data remains Chinese");
    test_ok("memo.convert", args); test_ok("memo.open", args); test_capture("MemoChecklist", "未完成", "Not done");
    test_ok("memo.delete", args); test_capture("MemoUndo", "撤销删除", "Undo delete");
    test_ok("memo.undo", args); test_open("memo"); test_capture("Memo", "说一句", "Voice note");
    test_ok("memo.edit", NULL); reject_note = true;
    test_ok("text.insert", "{\"text\":\"Synthetic unsaved note\"}");
    CHECK(WAIT_FOR(M.mode == FAIL, 15000), "memo enters save-failure view");
    test_capture("MemoSaveFail", "没存上", "Not saved"); reject_note = false;
    test_ok("memo.retry", NULL); CHECK(WAIT_FOR(!M.saving && ink_db_idle(), 15000), "retry saves");
    test_ok("text.submit", NULL); test_open("memo");
    test_ok("net.set_airplane", "{\"enabled\":true}"); CHECK(WAIT_FOR(!ink_hub_online(), 5000), "offline recording fixture");
    test_ok("memo.record", NULL); CHECK(WAIT_FOR(ink_audio_recording(), 5000), "recording starts");
    test_capture("MemoRecord", "正在记", "Recording");
    test_ok("audio.record_stop", NULL); CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "MemoPending"), 15000), "recording saved");
    test_capture("MemoPending", "录音已保存", "Recording saved");
}
#else
static void test_blur(void) { ink_text_blur(0); }
static remote_page_t test_remote_page;
static void test_remote_state(void) { page_show(test_remote_page); }
static void test_remotes(void) {
    test_ok("remote.open", NULL); CHECK(WAIT_FOR(ink_text_focused(), 5000), "remote address keyboard ready");
    test_capture("RemoteAddress", "输入网址", "Enter address"); on_ui(test_blur); test_quiet();
    test_ok("remote.open", "{\"url\":\"example.com\"}");
    CHECK(WAIT_FOR(RS.first_shown, 15000), "synthetic remote image arrives"); test_capture("Remote", "网址", "Address");
    const remote_page_t states[] = {LOADING, TYPE, BLOCKED, OFFLINE};
    const char *names[] = {"RemoteLoading", "RemoteType", "RemoteBlocked", "RemoteOffline"};
    const char *zh[] = {"正在打开", "输入文字", "这个网页打不开", "网页断开了"};
    const char *en[] = {"Opening...", "Enter text", "Can't open this page", "Page disconnected"};
    for (unsigned i = 0; i < 4; i++) {
        test_remote_page = states[i]; on_ui(test_remote_state); test_capture(names[i], zh[i], en[i]);
        if (ink_text_focused()) on_ui(test_blur);
        test_quiet();
    }
    RS.ended = true; test_remote_page = OFFLINE; on_ui(test_remote_state);
    test_capture("RemoteEnded", "网页已关闭", "Page closed"); test_ok("remote.close", NULL);
}
static void test_apps(void) {
    test_open("calculator"); test_capture("Calculator", "计算器", "Calculator");
    CHECK(AI("calc.eval", "{\"expr\":\"1/0\"}") && g_rc.status == INK_RC_INVALID, "calculator zero division");
    test_capture("Calculator-error", "不能除以 0", "Can't divide by 0");
    test_open("clock"); test_capture("Clock-empty", "还没有闹钟", "No alarms yet");
    test_ok("clock.new", NULL); test_capture("ClockAlarm-new", "保存闹钟", "Save alarm");
    test_ok("clock.save", NULL); test_open("clock"); test_capture("Clock", "查看闹钟", "View alarm");
    test_ok("clock.open", "{\"id\":1}"); test_capture("ClockAlarm", "删除闹钟", "Delete alarm");
    test_open("clock"); test_ok("clock.timer", NULL); test_capture("ClockTimer", "设定时长", "Set duration");
    test_ok("clock.stopwatch", NULL); test_capture("ClockStopwatch", "秒表", "Stopwatch");
    test_ok("clock.world", NULL); test_capture("ClockWorld-empty", "世界时钟", "World Clock");
    test_ok("clock.world_pick", NULL); test_capture("ClockWorld-pick", "选好了", "Done");
    test_ok("clock.world_add", "{\"zone\":7}"); test_ok("clock.world_pick", NULL);
    test_capture("ClockWorld", "世界时钟", "World Clock");
    test_open("phone"); test_ok("phone.contacts", NULL);
    CHECK(WAIT_FOR(!ink_ui_busy() && ink_db_idle(), 15000), "contacts loaded"); test_capture("PhoneContacts-empty", "联系人", "Contacts");
    test_ok("contacts.edit", NULL); test_capture("ContactEdit-new", "姓名", "Name");
    test_ok("contacts.put", "{\"name\":\"用户原文 晴\",\"number\":\"00000000001\"}");
    test_open("phone"); test_ok("phone.contacts", NULL); test_capture("PhoneContacts", "用户原文 晴", "用户原文 晴");
    test_ok("contacts.edit", "{\"id\":1}"); test_capture("ContactEdit", "保存联系人", "Save contact");
    test_open("phone"); test_capture("Phone", "拨号", "Dial");
    test_ok("phone.recents", NULL); test_capture("PhoneRecents-empty", "最近通话", "Recent calls");
    test_open("phone"); CHECK(WAIT_FOR(test_registered(), 15000), "synthetic modem registered");
    test_ok("call.dial", "{\"number\":\"00000000001\"}");
    CHECK(WAIT_FOR(sim_net_call(NULL, 0) == 1, 10000), "synthetic call connected");
    test_capture("PhoneCall", "通话中", "In call"); test_ok("phone.keypad", NULL); test_capture("PhoneKeypad", "收起键盘", "Hide keypad");
    test_ok("call.hangup", NULL); test_open("phone"); test_ok("phone.recents", NULL); test_capture("PhoneRecents", "打出", "Outgoing");
    test_open("message"); test_capture("Sms-empty", "还没有短信", "No messages yet");
    test_fixture("sms.messages", "{\"peer\":\"00000000001\",\"text\":\"用户原文 晴\",\"status\":\"收到\"}");
    test_open("message"); test_capture("Sms", "短信", "Messages"); test_ok("sms.thread", "{\"peer\":\"00000000001\"}");
    test_capture("SmsThread", "收到", "Received"); CHECK(strstr(page_json, "用户原文 晴"), "message body unchanged");
    test_ok("sms.compose", NULL); test_capture("SmsCompose", "写短信", "New message");
    test_ok("sms.long", NULL); test_capture("ImeLong", "写短信", "New message");
    test_open("message"); test_ok("sms.compose", NULL); test_capture("SmsRecipients", "用户原文 晴", "用户原文 晴");
    test_open("gallery"); test_capture("Gallery-empty", "相册", "Photos");
    test_open("camera"); test_ok("camera.preview", NULL); test_capture("Camera", "低频取景", "Still preview");
    CHECK(BY_HAND("camera.scan", NULL) && g_rc.status == INK_RC_FAILED, "synthetic image has no QR code"); test_quiet(); test_capture("Scan", "扫码", "Scan");
    test_open("camera"); test_ok("camera.photo", NULL);
    ink_fs_entry_t photo; uint32_t cursor = 0;
    CHECK(ink_fs_index("/me", INK_FS_IMAGE, &cursor, &photo) == 1, "synthetic photo saved");
    test_open("gallery"); test_capture("Gallery", "相册", "Photos"); char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", photo.id);
    test_ok("photo.open", args); test_capture("Photo", "问这张图", "Ask about photo");
    test_open("weather"); test_capture("Weather-empty", "还没有设天气城市", "No weather city set");
    test_ok("weather.city", NULL); test_capture("WeatherCity", "输入城市名", "Enter city name");
    test_ok("weather.find", "{\"name\":\"示例市\"}"); test_capture("WeatherCity-results", "选一个城市", "Choose a city");
    test_ok("weather.set_city", "{\"id\":101,\"name\":\"示例市\"}"); test_ok("weather.open", NULL); test_ok("weather.get", NULL);
    test_capture("Weather", "最高", "H 15");
    test_ok("net.set_airplane", "{\"enabled\":true}"); CHECK(WAIT_FOR(!ink_hub_online(), 5000), "weather offline");
    test_ok("weather.get", NULL); test_capture("Weather-offline", "没联网", "Offline");
    test_ok("net.set_airplane", "{\"enabled\":false}"); CHECK(WAIT_FOR(ink_hub_online(), 15000), "remote hub online");
    test_home(); test_remotes();
    test_home(); ink_power_lock(); CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 5000), "lock for public entry pages");
    test_ok("phone.emergency_open", NULL); overlay_mode = true;
    test_capture("PhoneEmergency", "紧急呼叫", "Emergency call"); test_ok("phone.emergency_close", NULL);
    test_ok("app.open", "{\"id\":\"camera\"}"); test_ok("camera.preview", NULL);
    test_capture("Camera-locked", "取景", "preview"); test_ok("camera.photo", NULL);
    test_capture("Camera-locked-photo", "本次", "photo"); test_ok("shell.back", NULL); overlay_mode = false;
    CHECK(ink_power_unlock(osal_now_us()), "unlock after public camera"); test_quiet();
}
#endif
static void run_test(void *arg) {
    (void)arg; vtk_host_call(test_mode, NULL); test_quiet(); test_start_home();
    test_ok("settings.set", english ? "{\"id\":\"system.language\",\"value\":1}" : "{\"id\":\"system.language\",\"value\":0}");
    missing_before = sim_metric_value("i18n.missing"); ink_power_hold(INK_NEED_AWAKE);
#ifdef INK_I18N_MEMO
    test_memos();
#else
    test_apps();
    test_open("calculator"); test_ok("settings.set", "{\"id\":\"system.language\",\"value\":0}");
    english = false; test_capture("Calculator-back-zh", "不能除以 0", "Can't divide by 0");
#endif
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "i18n-app-test", .fn = run_test, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start app language test");
}
