/* phone-3 acceptance on the whole product with the simulated AT modem: Fossify Phone's path (type, pick, dial, connect,
 * controls, hang up, back to recent calls), the four ways a call fails to connect, Lineage's name rule, the AI path,
 * recent call groups and details, the notice's place, touch targets, refresh counts, and pictures of every page at the
 * default text size, the largest one and as a child (INK_PHONE3_SET = 1, 3 or kid). */
#include <stdlib.h>
#ifndef INK_PHONE_SOURCE
#define INK_PHONE_SOURCE "../../system/apps/phone/phone.c"
#endif
#include INK_PHONE_SOURCE
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char set[8], snap[8192], shot_name[48];
static const char *label, *bound;
static lv_obj_t *found;
static bool full;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "the phone settles"); }
static void read_env(void *arg) { (void)arg; const char *s = getenv("INK_PHONE3_SET"); snprintf(set, sizeof set, "%s", s ? s : "1"); }
static void read_page(void) { CHECK(ink_ui_snapshot(snap, sizeof snap, NULL) > 0, "the page has a snapshot"); }
static void inspect(void) { on_ui(read_page); }
static void find_control(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if ((label && lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), label)) ||
        (bound && ink_shell_bound_cap(o) && !strcmp(ink_shell_bound_cap(o), bound))) found = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_control(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) {
    found = NULL; lv_obj_update_layout(lv_screen_active()); find_control(lv_screen_active()); find_control(lv_layer_sys());
    if (!found) return;
    lv_area_t r; lv_obj_get_coords(found, &r);
    g_btn_x[0] = (r.x1 + r.x2) / 2; g_btn_y[0] = (r.y1 + r.y2) / 2;
}
static bool click(const char *text, const char *cap) {
    label = text; bound = cap; on_ui(locate);
    CHECK(found, "visible control: %s", text ? text : cap);
    if (found) tap(g_btn_x[0], g_btn_y[0]);
    quiet();
    return found != NULL;
}
static bool page(const char *name) { return !strcmp(ink_ui_page_name(), name); }
static double refreshes(void) { /* display.dispatch.* counts every refresh sent to the panel */
    static const char *const intents[] = {"A2", "FAST", "UI", "TEXT", "FLASH_UI", "FULL"};
    double total = 0; char key[64];
    for (unsigned i = 0; i < 6; i++) {
        snprintf(key, sizeof key, "display.dispatch.%s", intents[i]); total += sim_metric_value(key);
        snprintf(key, sizeof key, "display.dispatch.%s+clean", intents[i]); total += sim_metric_value(key);
    }
    return total;
}
static double mark, marks[12];
static const char *const INTENTS[] = {"A2", "FAST", "UI", "TEXT", "FLASH_UI", "FULL", "A2+clean", "FAST+clean", "UI+clean", "TEXT+clean", "FLASH_UI+clean", "FULL+clean"};
static double intent_count(int i) { char key[64]; snprintf(key, sizeof key, "display.dispatch.%s", INTENTS[i]); return sim_metric_value(key); }
static uint32_t frames;
static void begin(void) { quiet(); mark = refreshes(); frames = ink_display_commits(); for (int i = 0; i < 12; i++) marks[i] = intent_count(i); }
/* What an action cost: frames drawn (ink_display_commits, the count other tests use), and the panel dispatches of each
 * waveform for the report (one frame is split into a fast and a normal dispatch when, say, the status bar changes too). */
static void count(const char *action, double expected) {
    quiet(); double n = ink_display_commits() - frames; char key[64]; snprintf(key, sizeof key, "test.refresh.%s", action);
    ink_probe_value(key, n);
    snprintf(key, sizeof key, "test.dispatch.%s", action); ink_probe_value(key, refreshes() - mark);

    for (int i = 0; i < 12; i++) if (intent_count(i) > marks[i]) {
        snprintf(key, sizeof key, "test.refresh.%s.%s", action, INTENTS[i]); ink_probe_value(key, intent_count(i) - marks[i]);
    }
    CHECK(n == expected, "%s costs %g frame(s), expected %g", action, n, expected);
}
/* every touch target at least 108 px and on the screen; no drawn circle (rings or black discs larger than a page dot) */
static void walk(lv_obj_t *o, lv_area_t screen) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    lv_area_t a; lv_obj_get_coords(o, &a);
    int w = lv_area_get_width(&a), h = lv_area_get_height(&a);
    /* below the shell's status bar (its indicator glyphs are not touch targets of this page) */
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE) && o != lv_screen_active() && lv_obj_get_parent(o) && a.y1 >= ink_ui_px(INK_STATUS_BAR)) {

        CHECK(w >= ink_ui_px(INK_CONTROL) - 1 && h >= ink_ui_px(INK_CONTROL) - 1, "%s: touch target %dx%d at %d,%d is 108 px", shot_name, w, h, (int)a.x1, (int)a.y1);
        CHECK(a.x1 >= screen.x1 && a.y1 >= screen.y1 && a.x2 <= screen.x2 && a.y2 <= screen.y2, "%s: touch target at %d,%d is on the screen", shot_name, (int)a.x1, (int)a.y1);
    }
    bool ring = lv_obj_get_style_radius(o, 0) == LV_RADIUS_CIRCLE && w > ink_ui_px(INK_PAGE_DOT) * 2 &&
        (lv_obj_get_style_border_width(o, 0) > 0 || (lv_obj_get_style_bg_opa(o, 0) > LV_OPA_50 && lv_color_brightness(lv_obj_get_style_bg_color(o, 0)) < 128));
    CHECK(!ring, "%s: no circle at %d,%d", shot_name, (int)a.x1, (int)a.y1);
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) walk(lv_obj_get_child(o, (int32_t)i), screen);
}
static void targets(void) {
    lv_area_t screen = {0, 0, ink_ui_width() - 1, ink_ui_height() - 1};
    lv_obj_update_layout(lv_screen_active()); walk(lv_screen_active(), screen);
}
static void capture(void *arg) { (void)arg; sim_display_snapshot(shot_name); }
static void shot(const char *name) {
    quiet(); snprintf(shot_name, sizeof shot_name, "%s-%s", name, set);
    on_ui(targets); vtk_event_at(vtk_now() + SIM_MS(1), capture, NULL); delay_ms(5);
}
static void world(void (*fn)(void *)) { vtk_event_at(vtk_now() + SIM_MS(1), fn, NULL); delay_ms(10); }
static void w_auto_off(void *a) { (void)a; sim_net_auto_answer(false); }
static void w_answer(void *a) { (void)a; sim_net_remote_answer(); }
static void w_busy(void *a) { (void)a; sim_net_remote_busy(); }
static void w_no_answer(void *a) { (void)a; sim_net_remote_no_answer(); }
static void w_drop(void *a) { (void)a; sim_net_remote_hangup(); }
static void w_headset(void *a) { (void)a; sim_headset_add("00:11:22:33:44:55", "合成耳机"); }
static void put_contact(const char *name, const char *number) {
    char args[256]; snprintf(args, sizeof args, "{\"name\":\"%s\",\"number\":\"%s\"}", name, number);
    CHECK(AI("contacts.put", args) && g_rc.status == INK_RC_DONE, "synthetic contact %s", name);
}
static void open_dial(const char *number) {
    char args[64]; snprintf(args, sizeof args, "{\"number\":\"%s\"}", number);
    CHECK(BY_HAND("phone.open", args) && g_rc.status == INK_RC_DONE, "open the dial pad"); quiet();
}
static const char *key_text;
static void find_key(lv_obj_t *o) { /* a dial key: its digit's label inside a key-wide button (the field shows digits too) */
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), key_text) && lv_obj_get_width(lv_obj_get_parent(o)) == ink_ui_px(INK_DIGIT)) found = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_key(lv_obj_get_child(o, (int32_t)i));
}
static void locate_key(void) {
    found = NULL; lv_obj_update_layout(lv_screen_active()); find_key(lv_screen_active());
    if (found) { lv_area_t r; lv_obj_get_coords(lv_obj_get_parent(found), &r); g_btn_x[0] = (r.x1 + r.x2) / 2; g_btn_y[0] = (r.y1 + r.y2) / 2; }
}
static void press_key(char d) {
    char k[2] = {d, 0}; key_text = k; on_ui(locate_key);
    CHECK(found, "dial key %s", k);
    if (found) tap(g_btn_x[0], g_btn_y[0]);
    quiet();
}
static void keys(const char *digits) { for (const char *d = digits; *d; d++) press_key(*d); }
static void dialled(void) {
    CHECK(WAIT_FOR(page("PhoneCall") && sim_net_call(NULL, 0) == 3, 10000), "the modem accepted the dial and the call page is up"); quiet();
}
static void call_over(void) { CHECK(WAIT_FOR(!H.active && !P.active, 10000) && WAIT_FOR(ink_db_idle(), 10000), "the call ended and its record is written"); quiet(); }
static bool last_result(const char *result) {
    ink_db_t *db = ink_db_open_member(&HISTORY, ink_member_current()->id);
    ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec; char raw[256] = {0}, want[48];
    bool ok = db && ink_db_next(db, &cur, &rec, raw, sizeof raw - 1) == 1;
    snprintf(want, sizeof want, "\"result\":\"%s\"", result);
    ink_db_close(db);
    return ok && strstr(raw, want);
}
static void tick(void) { ink_state_set_int(ink_state_find("time.minute"), (int)(ink_now_us() / 60000000) + 900); }

/* Fossify's path: type, several candidates (from the start first), fewer, pick (no call), dial (not shown as connected
 * while it rings), connect, a minute, mute, headset, keypad, hang up at 1:05, the result stays, 完成 to recent calls. */
static void fossify_path(void) {
    open_dial(""); shot("dial-empty");
    begin(); press_key('1'); count("key", 1);
    keys("38"); inspect();
    CHECK(strstr(snap, "1/2 翻页") && strstr(snap, "妈妈") && strstr(snap, "爸爸") && !strstr(snap, "王老师"), "138: two pages, the numbers starting 138 first: %.150s", snap);
    shot("dial-typing");
    begin(); CHECK(BY_HAND("phone.next", NULL) && g_rc.status == INK_RC_DONE, "turn the candidates"); count("page", 1); inspect();
    CHECK(strstr(snap, "王老师") && !strstr(snap, "妈妈"), "138 inside a number comes after the numbers starting 138");
    keys("12"); inspect();
    CHECK(strstr(snap, "妈妈") && strstr(snap, "王老师") && !strstr(snap, "爸爸") && !strstr(snap, "翻页"), "13812: fewer candidates");
    begin(); click("妈妈", NULL); count("pick", 1);
    CHECK(!strcmp(P.number, "138 1234 5678") && sim_net_call(NULL, 0) == 0, "a candidate fills the number and does not dial");
    begin(); click("拨打", NULL); dialled(); count("dial", 1);
    CHECK(!P.number[0], "Fossify clears the dial pad after dialling: here with the page change, no timer");
    inspect();
    CHECK(!strcmp(lv_label_get_text(P.status_line), "正在拨号") && strstr(snap, "正在拨号") && !strstr(snap, "通话中"), "accepted by the modem, not connected: %.200s", snap);
    shot("call-dialing");
    begin(); world(w_answer); CHECK(WAIT_FOR(P.started != 0, 5000), "the far end answers");
    count("connect", 2); /* the call page once (status line and duration); the shell's status bar adds its 通话中 mark in its own fast frame */
    CHECK(intent_count(2) - marks[2] == 1 && intent_count(1) - marks[1] == 1, "connect: one normal refresh of the page, one fast one of the status bar");
    inspect();
    CHECK(strstr(snap, "通话中") && !strcmp(lv_label_get_text(P.duration), "0 分钟"), "connected: 通话中 and 0 分钟");
    shot("call-active");
    begin(); click("静音", NULL); CHECK(WAIT_FOR(P.muted, 3000), "the modem confirms mute"); count("mute", 1);
    label = "已静音"; bound = NULL; on_ui(locate); CHECK(found, "the button now says 已静音");
    world(w_headset);
    CHECK(BY_HAND("bt.pair", "{\"addr\":\"00:11:22:33:44:55\"}") && BY_HAND("bt.connect", "{\"addr\":\"00:11:22:33:44:55\"}") && g_rc.status == INK_RC_DONE, "a headset connects");
    CHECK(WAIT_FOR(ink_audio_route() == INK_ROUTE_HEADSET, 3000), "the call moves to the headset"); quiet();
    label = "耳机"; on_ui(locate); CHECK(found, "the route button says 耳机");
    begin(); click("键盘", NULL); count("keypad_open", 1);
    click("1", NULL); CHECK(!strcmp(sim_net_call_tones(), "1"), "the keypad sends 1 through call.dtmf");
    shot("call-keypad");
    begin(); click(NULL, "phone.keypad"); count("keypad_close", 1);
    CHECK(ink_now_us() < P.started + 60000000ull, "the controls were tried within the first minute");
    CHECK(WAIT_FOR(ink_now_us() >= P.started + 60100000ull, 70000), "a minute passes");
    begin(); on_ui(tick); count("minute", 1);
    CHECK(!strcmp(lv_label_get_text(P.duration), "1 分钟"), "the minute changes only the duration");
    CHECK(WAIT_FOR(ink_now_us() >= P.started + 65000000ull, 10000) && ink_now_us() < P.started + 65900000ull, "1 min 05 s");
    begin(); click("挂断", NULL); call_over(); count("hangup", 2); /* like connect: the call page once, the status bar drops its 通话中 mark in its own fast frame */
    CHECK(intent_count(2) - marks[2] == 1 && intent_count(1) - marks[1] == 1, "hangup: one normal refresh of the page, one fast one of the status bar");
    CHECK(page("PhoneCall") && !strcmp(P.end_text, "通话结束 · 1 分 05 秒"), "the result stays: %s", P.end_text);
    CHECK(last_result("answered"), "the record says answered");
    shot("call-ended-answered");
    begin(); click("完成", NULL); CHECK(WAIT_FOR(P.list != NULL, 10000), "完成 opens recent calls"); count("recents", 1); inspect();
    CHECK(page("PhoneRecents") && strstr(snap, "妈妈") && strstr(snap, "1 分 05 秒"), "the top group is this call, in seconds");
}
static void fail_path(void (*how)(void *), bool self, const char *text, const char *result, const char *name) {
    open_dial("13700000001"); click("拨打", NULL); dialled();
    if (self) click("挂断", NULL); else world(how);
    call_over();
    CHECK(!strcmp(P.end_text, text) && page("PhoneCall"), "%s: %s (got %s)", name, text, P.end_text);
    CHECK(last_result(result), "%s: the record says %s", name, result);
    shot(name);
}
static void answered_quickly(void) { /* the other text sizes: a connected call's pages without the minute */
    open_dial("13812345678"); click("拨打", NULL); dialled();
    world(w_answer); CHECK(WAIT_FOR(P.started != 0, 5000), "connected"); shot("call-active");
    click("键盘", NULL); click("1", NULL); shot("call-keypad"); click(NULL, "phone.keypad");
    click("挂断", NULL); call_over(); shot("call-ended-answered");
}
static void names(void) { /* Lineage SmartDialNameMatcher:400-434 */
    open_dial("");
    for (const char *k = "846"; *k; k++) { char a[24]; snprintf(a, sizeof a, "{\"key\":\"%c\"}", *k); BY_HAND("phone.key", a); }
    quiet(); inspect(); CHECK(strstr(snap, "Phillips Thomas"), "846 finds Phillips Thomas (Tho...)");
    open_dial("");
    for (const char *k = "466"; *k; k++) { char a[24]; snprintf(a, sizeof a, "{\"key\":\"%c\"}", *k); BY_HAND("phone.key", a); }
    quiet(); inspect(); CHECK(!strstr(snap, "Phillips Thomas") && strstr(snap, "没有匹配的联系人"), "466 (inside a word) finds nothing");
}
static void ai_path(void) { /* the same capabilities from the AI; dialling still asks a person (D-105) */
    CHECK(AI("phone.open", "{}") && g_rc.status == INK_RC_DONE, "AI opens the dial pad"); quiet();
    for (const char *k = "138"; *k; k++) { char a[24]; snprintf(a, sizeof a, "{\"key\":\"%c\"}", *k); CHECK(AI("phone.key", a) && g_rc.status == INK_RC_DONE, "AI key"); }
    quiet(); inspect(); CHECK(strstr(snap, "妈妈") && strstr(snap, "phone.open") && strstr(snap, "1/2 翻页"), "AI sees the same candidates");
    CHECK(AI("phone.open", "{\"number\":\"138 1234 5678\"}") && g_rc.status == INK_RC_DONE, "AI picks a candidate"); quiet();
    CHECK(sim_net_call(NULL, 0) == 0 && !strcmp(P.number, "138 1234 5678"), "picking does not dial");
    CHECK(AI("call.dial", "{\"number\":\"138 1234 5678\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI dial waits for a person");
    ink_confirm_view_t pending[1];
    CHECK(ink_confirm_list(pending, 1) == 1 && ink_confirm_decide(pending[0].token, true, INK_SRC_TOUCH) == 0, "a person confirms");
    dialled(); inspect(); CHECK(strstr(snap, "正在拨号") && !strstr(snap, "通话中"), "AI path: not shown as connected");
    world(w_answer); CHECK(WAIT_FOR(P.started != 0, 5000), "connected"); quiet(); inspect();
    CHECK(strstr(snap, "通话中") && strstr(snap, "call.hangup"), "AI path: the same call snapshot");
    CHECK(AI("call.hangup", NULL) && g_rc.status == INK_RC_DONE, "AI hangs up"); call_over();
    CHECK(!strncmp(P.end_text, "通话结束", strlen("通话结束")), "AI path: the same result");
}
static void w_ring(void *a) { (void)a; sim_net_incoming_call("13600000002"); }
static bool on_screen(const char *text) { /* on the page itself, not on an overlay */
    label = text; bound = NULL; found = NULL; find_control(lv_screen_active()); return found != NULL;
}
static bool answer_shown;
static void look_answer(void) { answer_shown = on_screen("接听"); }
static void unsaved_and_ringing(void) {
    open_dial(""); click("拨打", NULL); /* nothing typed: Fossify's initCall fills the last number, no call (A12) */
    CHECK(!strcmp(P.number, "138 1234 5678") && sim_net_call(NULL, 0) == 0 && page("Phone"), "拨打 with nothing typed fills the last number: %s", P.number);
    open_dial(""); keys("1370"); inspect(); /* a number called but not saved (A6) */
    CHECK(strstr(snap, "13700000001") && strstr(snap, "最近通话"), "a number called before and not saved is a candidate, marked 最近通话");
    world(w_ring); CHECK(WAIT_FOR(P.active && P.incoming, 10000), "a call rings"); quiet();
    CHECK(BY_HAND("phone.return", NULL) && g_rc.status == INK_RC_DONE, "open the call page while it rings"); quiet();
    on_ui(look_answer); CHECK(page("PhoneCall") && answer_shown, "LilyPhone: 接听 shows while it rings (B6)");
    CHECK(BY_HAND("call.answer", NULL) && g_rc.status == INK_RC_DONE && WAIT_FOR(P.started != 0, 5000), "answered"); quiet();
    on_ui(look_answer); CHECK(!answer_shown || lv_obj_has_flag(lv_obj_get_parent(found), LV_OBJ_FLAG_HIDDEN), "接听 goes once answered");
    CHECK(BY_HAND("call.hangup", NULL) && g_rc.status == INK_RC_DONE, "hang up"); call_over();
}
static void add_record(const char *raw, int64_t at) {

    ink_db_t *db = ink_db_open_member(&HISTORY, ink_member_current()->id);
    ink_db_write_t w = {.sort = at, .data = raw, .len = strlen(raw), .src = INK_SRC_SYSTEM};
    CHECK(db && ink_db_put(db, &w, NULL) == INK_DB_OK, "synthetic record");
    ink_db_flush(db); ink_db_close(db);
}
static int records(void) { ink_db_t *db = ink_db_open_member(&HISTORY, ink_member_current()->id); int n = db ? ink_db_count(db) : -1; ink_db_close(db); return n; }
static lv_obj_t *detail_button;
static void find_detail(void) { /* the 详情 beside the row whose title is `label` */
    detail_button = NULL; locate();
    if (found) detail_button = lv_obj_get_child(lv_obj_get_parent(found), 2);
    if (detail_button) { lv_area_t r; lv_obj_get_coords(detail_button, &r); g_btn_x[0] = (r.x1 + r.x2) / 2; g_btn_y[0] = (r.y1 + r.y2) / 2; }
}
static void recents(void) {
    int64_t now = ink_time_now_s();
    char raw[200];
    snprintf(raw, sizeof raw, "{\"number\":\"13700000001\",\"kind\":\"打出\",\"minutes\":0,\"seconds\":12,\"result\":\"answered\",\"sim\":\"external\",\"at\":%lld}", (long long)(now - 86400));
    add_record(raw, now - 86400);
    snprintf(raw, sizeof raw, "{\"number\":\"10086\",\"kind\":\"打出\",\"minutes\":2,\"at\":%lld}", (long long)(now - 3 * 86400)); /* the old format: minutes only */
    add_record(raw, now - 3 * 86400);

    CHECK(BY_HAND("phone.recents", NULL) && g_rc.status == INK_RC_DONE, "recent calls"); quiet();
    CHECK(WAIT_FOR(P.list != NULL, 10000), "history loaded"); quiet(); inspect();
    CHECK(strstr(snap, "13700000001 (4)"), "four calls in a row, same day, same number: one group (4): %.150s", strstr(snap, "13700000001") ? strstr(snap, "13700000001") : snap);
    CHECK(X.ngroups == 6 && X.groups[2].count == 4 && X.groups[4].count == 1, "groups: 136, 妈妈, 137 (4), 妈妈, 137 yesterday, 10086");
    begin(); key(INK_KEY_NEXT); count("recents_page", 1); inspect();
    CHECK(intent_count(2) - marks[2] == 1, "a cover-key page turn redraws the list once");
    CHECK(strstr(snap, "昨天"), "yesterday's call is a group of its own: %.150s", strstr(snap, "\"fields\"") ? strstr(snap, "\"fields\"") : snap);
    CHECK(strstr(snap, "10086") && strstr(snap, "2 分 00 秒"), "the old record (minutes only) reads 2 min 00 s");
    begin(); key(INK_KEY_NEXT); count("cover_key_last_page", 0); /* nothing to turn: what the key itself costs */
    key(INK_KEY_PREV); quiet();
    click("13700000001 (4)", NULL);
    CHECK(WAIT_FOR(page("Phone"), 5000) && !strcmp(P.number, "13700000001") && sim_net_call(NULL, 0) == 0, "a group fills the dial pad, no call");
    CHECK(BY_HAND("phone.recents", NULL), "back to recent calls"); quiet();
    label = "13700000001 (4)"; bound = NULL; on_ui(find_detail); CHECK(detail_button != NULL, "the group has its 详情");
    if (detail_button) tap(g_btn_x[0], g_btn_y[0]);
    quiet(); CHECK(WAIT_FOR(page("PhoneDetail"), 5000), "详情 opens the details"); inspect();
    CHECK(strstr(snap, "\"total\":4") && strstr(snap, "打电话") && strstr(snap, "发短信") && strstr(snap, "加为联系人") && strstr(snap, "删除这一组"), "the details list the four calls and the actions: %.150s", snap);
    shot("detail");
    click("发短信", NULL); inspect(); /* D3: sms.compose with the number */
    CHECK(!page("PhoneDetail") && strstr(snap, "13700000001"), "发短信 opens the message editor with the number: %.150s", snap);
    CHECK(BY_HAND("phone.recents", NULL), "back to recent calls"); quiet();
    label = "13700000001 (4)"; bound = NULL; on_ui(find_detail); if (detail_button) tap(g_btn_x[0], g_btn_y[0]);
    quiet(); CHECK(page("PhoneDetail"), "the details again");
    click("打电话", NULL); dialled(); CHECK(sim_net_call(NULL, 0) == 3, "打电话 dials"); click("挂断", NULL); call_over();
    CHECK(BY_HAND("phone.recents", NULL), "recent calls again"); quiet();
    int before = records();
    label = "13700000001 (4)"; on_ui(find_detail); if (detail_button) tap(g_btn_x[0], g_btn_y[0]);
    quiet(); CHECK(page("PhoneDetail"), "details of the group of four");
    begin(); click("删除这一组", NULL); CHECK(WAIT_FOR(page("PhoneRecents") && ink_db_idle(), 5000), "deleted, back to recent calls"); count("delete", 1); quiet();
    CHECK(records() == before - 4 && strstr(lv_label_get_text(P.notice), "已删除"), "four records gone, 已删除 with 撤销, no question asked");
    shot("recents-undo");
    begin(); click("撤销", NULL); CHECK(WAIT_FOR(records() == before, 5000), "撤销 brings them back"); count("undo", 1); quiet();
    click("清空", NULL);
    ink_confirm_view_t pending[1];
    CHECK(WAIT_FOR(ink_confirm_list(pending, 1) == 1, 3000) && records() == before, "清空 asks first (it cannot be undone)");
    CHECK(ink_confirm_decide(pending[0].token, true, INK_SRC_TOUCH) == 0 && WAIT_FOR(records() == 0, 5000), "clear after yes"); quiet();
    CHECK(page("PhoneRecents") && WAIT_FOR(P.list != NULL, 5000), "the cleared history"); inspect();
    CHECK(strstr(snap, "\"total\":0"), "nothing left: the shared empty page"); shot("recents-empty");
}
/* phone-3 review: phone.delete takes a group and nothing else, never more than undo keeps (64 calls). */
static uint32_t put_call(const char *number, int64_t at) {
    char raw[200]; uint32_t id = 0;
    snprintf(raw, sizeof raw, "{\"number\":\"%s\",\"kind\":\"打出\",\"seconds\":1,\"result\":\"answered\",\"sim\":\"external\",\"at\":%lld}", number, (long long)at);
    ink_db_t *db = ink_db_open_member(&HISTORY, ink_member_current()->id);
    ink_db_write_t w = {.sort = at, .data = raw, .len = strlen(raw), .src = INK_SRC_SYSTEM};
    CHECK(db && ink_db_put(db, &w, &id) == INK_DB_OK, "synthetic record %s", number);
    ink_db_flush(db); ink_db_close(db);
    return id;
}
static void delete_bounds(void) {
    int64_t now = ink_time_now_s();
    put_call("10010", now - 600); put_call("10010", now - 500);
    put_call("13700000009", now - 400); put_call("13700000009", now - 300); uint32_t head = put_call("13700000009", now - 200);
    CHECK(WAIT_FOR(ink_db_idle(), 5000) && records() == 5, "two calls to 10010, then three to 13700000009");
    char args[96]; snprintf(args, sizeof args, "{\"id\":%u,\"count\":500}", (unsigned)head);
    CHECK(AI("phone.delete", args) && g_rc.status == INK_RC_INVALID && records() == 5, "a count above what undo keeps is refused: nothing deleted");
    snprintf(args, sizeof args, "{\"id\":%u,\"count\":5}", (unsigned)head);
    CHECK(AI("phone.delete", args) && g_rc.status == INK_RC_DONE && g_rc.undo_ms > 0, "AI deletes the group, undoable");
    CHECK(WAIT_FOR(ink_db_idle(), 5000) && records() == 2, "only the group's three calls went, not the two older calls to 10010: %d left", records());
    char undo[96]; snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, (unsigned)g_rc.revision);
    CHECK(BY_HAND("system.undo", undo) && g_rc.status == INK_RC_UNDONE && WAIT_FOR(records() == 5, 5000), "撤销 brings the three back (%d, %s)", g_rc.status, g_rc.say);
    for (int i = 0; i < 70; i++) put_call("13900000007", now - 100 + i); /* 70 in a row, same day, same number */
    CHECK(BY_HAND("phone.recents", NULL) && g_rc.status == INK_RC_DONE, "recent calls"); quiet();
    CHECK(WAIT_FOR(P.list != NULL, 10000) && X.groups[0].count == 64 && X.groups[1].count == 6, "a group holds at most 64 calls: %d, %d", X.groups[0].count, X.groups[1].count);
    snprintf(args, sizeof args, "{\"id\":%u,\"count\":64}", (unsigned)X.calls[0].id);
    CHECK(AI("phone.delete", args) && g_rc.status == INK_RC_DONE && WAIT_FOR(records() == 11, 5000), "the group of 64 goes");
    snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, (unsigned)g_rc.revision);
    CHECK(BY_HAND("system.undo", undo) && g_rc.status == INK_RC_UNDONE && WAIT_FOR(records() == 75, 5000), "and all 64 come back (%d, %d %s)", records(), g_rc.status, g_rc.detail);
    quiet();
}
/* phone-3 review: no max_records (an old backup with more than 500 calls must restore); each new call trims to 500. */
static volatile int bg_done, bg_import;
static ink_exec_job_t bg_job;
static void reimport(void *arg) { /* BG: the backup service's export and import of this member's history */
    (void)arg;
    ink_db_t *db = ink_db_open_member(&HISTORY, ink_member_current()->id);
    void *data = NULL; size_t len = 0;
    int exported = -1; /* refused while the collection folds its log: again a little later, as the backup service does */
    for (int i = 0; db && exported && i < 100; i++) if ((exported = ink_db_export(db, &data, &len))) osal_task_delay_us(100000);
    bg_import = !db ? -7 : exported ? -9 : ink_db_import(db, data, len, ink_db_last_ver(db));
    osal_free(data); ink_db_close(db);
    __atomic_store_n(&bg_done, 1, __ATOMIC_RELEASE);
}
static void history_cap(void) {
    ink_db_t *db = ink_db_open_member(&HISTORY, ink_member_current()->id); ink_db_cursor_t cur = {0}; ink_rec_t rec;
    while (db && ink_db_next(db, &(ink_db_cursor_t){0}, &rec, NULL, 0) == 1) ink_db_del(db, rec.id, INK_SRC_SYSTEM); /* start empty */
    for (int i = 0; db && i < 600; i++) {
        char bulk[160]; snprintf(bulk, sizeof bulk, "{\"number\":\"1380000%04d\",\"kind\":\"打出\",\"seconds\":1,\"at\":%d}", i, i);
        ink_db_write_t w = {.sort = 1000 + i, .data = bulk, .len = strlen(bulk), .src = INK_SRC_SYSTEM};
        CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK, "record %d of 600 accepted", i);
    }
    ink_db_flush(db); ink_db_close(db);
    CHECK(WAIT_FOR(ink_db_idle(), 20000) && records() == 600, "an old backup's 600 records fit the collection: %d", records());
    bg_done = 0; ink_exec_post(INK_EXEC_BG, &bg_job, reimport, NULL);
    CHECK(WAIT_FOR(bg_done, 20000) && bg_import == 0 && records() == 600, "a snapshot of 600 records restores (import: %d)", bg_import);
    open_dial("13700000001"); click("拨打", NULL); dialled(); click("挂断", NULL); call_over();
    CHECK(records() == 500, "a new call keeps the newest 500: %d", records());
    db = ink_db_open_member(&HISTORY, ink_member_current()->id);
    CHECK(db && ink_db_next(db, &cur, &rec, NULL, 0) == 1 && rec.sort == 1101, "the 101 oldest went first: %lld", (long long)rec.sort);
    ink_db_close(db);
    CHECK(ink_db_find_def("phone.calls")->schema == 1 && !ink_db_find_def("phone.calls")->max_records, "schema 1 and no record limit: old backups still restore");
}
/* phone-3 review: the NT26's call reports, unverified on the device, each one the simulated modem can do instead. */
static unsigned quirks_now;
static void w_quirks(void *a) { (void)a; sim_net_call_quirks(quirks_now); }
static void quirk(unsigned q) { quirks_now = q; world(w_quirks); }
static void quirks(void) {
    quirk(SIM_CALL_NO_CLCC_URC); /* no unsolicited +CLCC: answered is found by AT+CLCC once a second */
    double connected = sim_metric_value("event.call.connected");
    open_dial("13700000001"); click("拨打", NULL); dialled();
    world(w_answer); CHECK(WAIT_FOR(P.started != 0, 5000), "answered without +CLCC from the modem: polling finds it"); quiet(); inspect();
    CHECK(strstr(snap, "通话中") && sim_metric_value("event.call.connected") - connected == 1, "通话中, call.connected once");
    click("挂断", NULL); call_over();
    CHECK(!strncmp(P.end_text, "通话结束", strlen("通话结束")) && last_result("answered"), "a talked call ends as answered: %s", P.end_text);
    quirk(SIM_CALL_ATD_BUSY); /* BUSY as ATD's own result: placed, then over at once, in that order */
    open_dial("13700000001"); click("拨打", NULL);
    CHECK(WAIT_FOR(!H.active && !P.active && page("PhoneCall") && ink_db_idle(), 10000) && !strcmp(P.end_text, "对方忙或拒接"), "ATD answers BUSY: 对方忙或拒接 (%s)", P.end_text);
    CHECK(last_result("busy") && !P.say[0], "the record says busy, and the dial did not fail");
    quirk(SIM_CALL_ATD_NO_ANSWER);
    open_dial("13700000001"); click("拨打", NULL);
    CHECK(WAIT_FOR(!H.active && !P.active && page("PhoneCall") && ink_db_idle(), 10000) && !strcmp(P.end_text, "没人接"), "ATD answers NO ANSWER: 没人接 (%s)", P.end_text);
    CHECK(last_result("no_answer"), "the record says no_answer");
    quirk(SIM_CALL_CHUP_NO_CARRIER); /* NO CARRIER ends AT+CHUP before its OK: still our hangup */
    open_dial("13700000001"); click("拨打", NULL); dialled(); click("挂断", NULL); call_over();
    CHECK(!strcmp(P.end_text, "已取消") && last_result("cancelled") && !P.say[0], "已取消, cancelled, no 没有完成 (%s / %s)", P.end_text, P.say);
    quirk(0);
}
/* phone-3 review: the call page names the caller from the current member's contacts, not the last member's. */
static void w_ring_mum(void *a) { (void)a; sim_net_incoming_call("13812345678"); }
static void w_ring_new(void *a) { (void)a; sim_net_incoming_call("13900000002"); }
static bool shown_mum, shown_number, shown_new;
static char name_field[64];
static void look_names(void) {
    shown_mum = on_screen("妈妈"); shown_number = on_screen("13812345678"); shown_new = on_screen("新同事"); name_field[0] = 0;
    for (size_t i = 0; i < P.model.field_count; i++)
        if (!strcmp(P.model.fields[i].id, "name")) snprintf(name_field, sizeof name_field, "%s", P.model.fields[i].value);
}
static void members(void) {
    open_dial("138"); quiet(); /* the owner's contacts are in the table now */
    CHECK(ink_member_switch("kid") == 0, "the child"); quiet();
    world(w_ring_mum); CHECK(WAIT_FOR(P.active && P.incoming, 10000), "a call rings for the child"); quiet();
    CHECK(BY_HAND("phone.return", NULL) && g_rc.status == INK_RC_DONE, "the child opens the call page"); quiet(); inspect();
    on_ui(look_names);
    CHECK(page("PhoneCall") && !shown_mum && shown_number && strcmp(name_field, "妈妈"), "the child's call page and its snapshot do not name the owner's contact (%s)", name_field);
    CHECK(BY_HAND("call.hangup", NULL) && g_rc.status == INK_RC_DONE, "the child rejects it"); call_over();
    CHECK(ink_member_switch("owner") == 0, "back to the owner"); quiet();
    put_contact("新同事", "13900000002");
    world(w_ring_new); /* the owner's contacts changed since the table was read */
    CHECK(WAIT_FOR(P.active && P.incoming, 10000), "a call from the new contact"); quiet();
    CHECK(BY_HAND("phone.return", NULL) && g_rc.status == INK_RC_DONE, "the call page"); quiet(); inspect();
    on_ui(look_names);
    CHECK(shown_new && !strcmp(name_field, "新同事"), "a contact added since names the caller (%s)", name_field);
    CHECK(BY_HAND("call.hangup", NULL) && g_rc.status == INK_RC_DONE, "reject"); call_over();
}
/* the notice has its own row: off the keys, on the screen, not over a contact's name */
static lv_area_t keys_area[12];
static void notice_place(void) {
    static const char *const k[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
    CHECK(P.notice && !lv_obj_has_flag(P.notice, LV_OBJ_FLAG_HIDDEN), "%s: the notice shows", shot_name);
    if (!P.notice) return;
    lv_area_t n; lv_obj_update_layout(lv_screen_active()); lv_obj_get_coords(P.notice, &n);
    CHECK(n.x1 >= 0 && n.y1 >= 0 && n.x2 < ink_ui_width() && n.y2 < ink_ui_height(), "%s: notice on the screen", shot_name);
    for (int i = 0; i < 12; i++) {
        label = k[i]; bound = NULL; found = NULL; find_control(lv_screen_active());
        if (!found) { CHECK(false, "key %s", k[i]); continue; }
        lv_obj_get_coords(lv_obj_get_parent(found), &keys_area[i]);
        lv_area_t *a = &keys_area[i];
        CHECK(n.y2 < a->y1 || n.y1 > a->y2 || n.x2 < a->x1 || n.x1 > a->x2, "%s: notice does not overlap key %s", shot_name, k[i]);
    }
    for (int s = 0; s < P.rows; s++) if (!lv_obj_has_flag(P.slots[s], LV_OBJ_FLAG_HIDDEN)) {
        lv_area_t a; lv_obj_get_coords(lv_obj_get_child(P.slots[s], 0), &a);
        CHECK(n.y2 < a.y1 || n.y1 > a.y2, "%s: notice is not over a name", shot_name);
        CHECK(a.y2 < keys_area[0].y1, "%s: a name is above the keys", shot_name);
    }
}
static void notices(void) {
    open_dial(""); press_key('*'); click("拨打", NULL);
    CHECK(WAIT_FOR(P.say[0] != 0, 5000), "a short reply arrives"); quiet();
    snprintf(shot_name, sizeof shot_name, "dial-error-%s", set); on_ui(notice_place); shot("dial-error");
    put_contact("一个非常非常长的名字用来检查候选行会不会压住键盘和提示文字", "13512345678");
    open_dial(""); keys("135"); press_key('*'); click("拨打", NULL);
    CHECK(WAIT_FOR(P.say[0] != 0, 5000), "a reply beside a long name"); quiet();
    snprintf(shot_name, sizeof shot_name, "dial-longname-%s", set); on_ui(notice_place);
    if (!strcmp(set, "kid")) return; /* a child does not switch airplane mode */
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "airplane mode on"); quiet();
    open_dial("10086"); click("拨打", NULL);
    CHECK(WAIT_FOR(strstr(P.say, "飞行模式"), 5000), "the longest reply (airplane mode): %s", P.say); quiet();
    snprintf(shot_name, sizeof shot_name, "dial-airplane-%s", set); on_ui(notice_place);
    click("取消", NULL);
    shot("dial-offline");
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE, "airplane mode off"); quiet();

}
static void run(void *arg) {
    (void)arg; vtk_host_call(read_env, NULL); full = !strcmp(set, "1");
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); quiet(); test_start_home(); quiet();
    ink_time_set(1793803620, false);
    if (!strcmp(set, "kid")) { CHECK(ink_member_switch("kid") == 0, "the child's phone"); quiet(); }
    if (!strcmp(set, "3")) CHECK(AI("settings.set", "{\"id\":\"display.font_size\",\"value\":3}") && g_rc.status == INK_RC_DONE, "largest text");
    quiet(); world(w_auto_off);
    CHECK(WAIT_FOR(sim_net_call(NULL, 0) == 0, 1000), "no call yet");
    put_contact("妈妈", "138 1234 5678"); put_contact("爸爸", "13800001111"); put_contact("王老师", "02013812345"); put_contact("Phillips Thomas", "+86 139 0000 1111");
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "contacts stored");
    if (full) fossify_path();
    else { open_dial(""); shot("dial-empty"); keys("138"); shot("dial-typing"); answered_quickly(); }
    open_dial(""); keys("9999"); inspect();
    CHECK(strstr(snap, "没有匹配的联系人"), "no match: %.150s", strstr(snap, "candidates") ? strstr(snap, "candidates") : snap);
    CHECK(strstr(snap, "contacts.edit"), "加为联系人: %.150s", strstr(snap, "\"actions\"") ? strstr(snap, "\"actions\"") : snap);
    shot("dial-nomatch");
    fail_path(w_busy, false, "对方忙或拒接", "busy", "call-ended-busy");
    fail_path(w_no_answer, false, "没人接", "no_answer", "call-ended-noanswer");
    fail_path(w_drop, false, "没有接通", "failed", "call-ended-failed");
    fail_path(NULL, true, "已取消", "cancelled", "call-ended-cancelled");
    if (full) { names(); ai_path(); unsaved_and_ringing(); }
    CHECK(BY_HAND("phone.recents", NULL), "recent calls"); quiet(); CHECK(WAIT_FOR(P.list != NULL, 10000), "history"); shot("recents");
    if (full) { recents(); delete_bounds(); quirks(); members(); history_cap(); }
    else {
        label = "13700000001 (4)"; bound = NULL; on_ui(find_detail);
        if (detail_button) tap(g_btn_x[0], g_btn_y[0]);
        quiet(); CHECK(page("PhoneDetail"), "details"); shot("detail");
    }
    notices();
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "no writes outstanding");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "phone-3-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the phone-3 acceptance driver");
}
