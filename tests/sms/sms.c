/* Exercise the real app and modem model; the mutant removes the member store boundary. */
#include <stdlib.h>
#include "ink/sys/data.h"
#ifdef INK_SMS_NO_MEMBER_CHECK
#define ink_db_open_cached_member(def, member) ink_db_open_cached_member(def, "owner")
#endif
#include "../../system/apps/sms/sms.c"
#undef ink_db_open_cached_member
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char phase[24], body[355], arguments[768], snap[4096], sent_text[400], sent_number[24];
static const char PEER[] = "00000000001";
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static bool registered(void) { int64_t value = 0; return ink_state_get_int(ink_state_find("cell.registered"), &value) && value; }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "SMS display settles"); }
static void read_phase(void *arg) { (void)arg; snprintf(phase, sizeof phase, "%s", getenv("INK_SMS_PHASE")); }
static void snapshot_page(void) {
    CHECK(ink_ui_snapshot(snap, sizeof snap, NULL) > 0, "real SMS page has an AI snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "SMS snapshot fits the production AI buffer");
}
static void open_list(void) {
    CHECK(BY_HAND("shell.launch", "{\"id\":\"message\"}") && g_rc.status == INK_RC_DONE, "existing system SMS launcher opens the real page");
    CHECK(WAIT_FOR(S.list != NULL, 10000), "member data loads on the background executor"); quiet(); on_ui(snapshot_page);
}
static int stored(const char *member) {
    ink_db_t *db = ink_db_open_member(ink_db_find_def("sms.messages"), member);
    int count = db ? ink_db_count(db) : -1; ink_db_close(db); return count;
}
static bool has_status(const char *status) {
    ink_db_t *db = ink_db_open_member(ink_db_find_def("sms.messages"), "owner"); ink_db_cursor_t cur = {0}; ink_rec_t rec;
    char peer[24], text[355], value[96]; bool found = false;
    while (db && ink_db_next(db, &cur, &rec, NULL, 0) == 1) {
        if (read_message(db, rec.id, peer, text, value) && !strcmp(value, status)) found = true;
    }
    ink_db_close(db); return found;
}
static bool durable(void) {
    ink_db_t *db = ink_db_open_member(ink_db_find_def("sms.messages"), "owner");
    bool done = db && ink_db_durable_ver(db) == ink_db_last_ver(db); ink_db_close(db); return done;
}
static void incoming(void *arg) { (void)arg; CHECK(sim_net_deliver_sms(PEER, "合成短信：回家后联系"), "modem accepts synthetic incoming SMS"); }
static void deliver(void) {
    int before = stored("owner"); vtk_event_at(vtk_now() + SIM_MS(1), incoming, NULL);
    CHECK(WAIT_FOR(stored("owner") == before + 1, 10000), "message.received reaches the durable conversation");
}
static void capture(void *arg) { (void)arg; CHECK(settled(), "capture is settled"); sim_display_snapshot("scene"); }
static void scene(const char *name) {
    quiet(); on_ui(snapshot_page); ink_probe_count("test.snapshot.settled");
    char counter[64]; snprintf(counter, sizeof counter, "test.snapshot.%s", name); ink_probe_count(counter);
    vtk_event_at(vtk_now() + SIM_MS(1), capture, NULL); delay_ms(5);
}
static void type_short(void) {
    CHECK(S.field && ink_text_focused(), "F2 input is focused");
    ink_text_commit_utf8("合成短短信"); ink_text_enter();
}
static void type_long(void) {
    CHECK(S.field && ink_text_focused(), "F3 long input is focused");
    ink_text_select(0, ink_text_char_offset(ink_text_value(), strlen(ink_text_value()))); ink_text_commit_utf8(body);
}
static void inspect_long(void) {
    CHECK(S.field && !strcmp(ink_textfield_text(S.field), body), "long input retains every Chinese character");
    CHECK(S.field && ink_textfield_long_snapshot(S.field, snap, sizeof snap) > 0 && strstr(snap, "ImeLong"), "long editor exposes its owned viewport");
}
static bool rendered_text(lv_obj_t *obj, const char *text) {
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), text)) {
        lv_area_t label, card; lv_obj_update_layout(obj); lv_obj_get_coords(obj, &label); lv_obj_get_coords(lv_obj_get_parent(obj), &card);
        return label.x1 >= card.x1 && label.x2 <= card.x2 && label.y1 >= card.y1 && label.y2 <= card.y2 &&
            lv_color_eq(lv_obj_get_style_text_color(obj, 0), ink_ui_paper());
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) if (rendered_text(lv_obj_get_child(obj, i), text)) return true;
    return false;
}
static void inspect_thread(void) {
    snapshot_page(); CHECK(S.list && S.count && strstr(snap, S.texts[0]) && strstr(snap, "已发出"), "thread snapshot shows the visible long SMS page and receipt (count=%zu visible=%zu page=%zu pages=%zu value=%.60s)", S.count, S.visible, S.position.page, S.thread_pages, S.texts[0]);
    CHECK(rendered_text(lv_screen_active(), S.texts[0]), "the visible long label fits inside its message card");
}
static void inspect_compose(void) { snapshot_page(); CHECK(S.field && (ink_textfield_text(S.field)[0] ? strstr(snap, "message.send") != NULL : strstr(snap, "message.send") == NULL) && strstr(snap, "00000000001"), "compose snapshot exposes its exact recipient and send capability"); }
static lv_obj_t *located;
static const char *wanted;
static void find_label(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) located = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, i));
}
static void locate(void) {
    located = NULL; find_label(lv_screen_active()); CHECK(located, "visible SMS button: %s", wanted);
    if (located) { lv_area_t a; lv_obj_update_layout(lv_obj_get_parent(located)); lv_obj_get_coords(lv_obj_get_parent(located), &a); g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2; }
}
static void tap_text(const char *label) { wanted = label; on_ui(locate); if (located) tap(g_btn_x[0], g_btn_y[0]); quiet(); }
static void remove_card(void *arg) { (void)arg; sim_net_set_card(0, false, false, false); }
static void replace_card(void *arg) { (void)arg; sim_net_set_card(0, true, true, true); sim_net_modem_power(false); sim_net_modem_power(true); }
static void no_registration(void) { vtk_event_at(vtk_now() + SIM_MS(1), remove_card, NULL); }
static void restore_registration(void) { vtk_event_at(vtk_now() + SIM_MS(1), replace_card, NULL); }
static void fail_modem(void *arg) { (void)arg; sim_net_set_card(0, true, false, true); }
static void restore_modem(void *arg) { (void)arg; sim_net_set_card(0, true, true, true); }
static volatile bool confirmation_done;
static char confirmation_status[24];
static void confirmed(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event; ink_args_get_str(a, "status", confirmation_status, sizeof confirmation_status); confirmation_done = true;
}
static void compose(void) {
    CHECK(AI("contacts.put", "{\"name\":\"合成收件人\",\"number\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "create only a synthetic contact");
    CHECK(BY_HAND("sms.compose", NULL) && g_rc.status == INK_RC_DONE, "compose opens recipient and body fields");
    CHECK(WAIT_FOR(S.recipient != NULL, 10000), "recipient input is immediately available"); quiet();
    CHECK(AI("sms.recipient", "{\"id\":1}") && g_rc.status == INK_RC_DONE, "existing contact selection capability remains compatible"); quiet(); CHECK(WAIT_FOR(S.field != NULL, 10000), "touch selection goes through contacts.select and opens the composer");
    CHECK(!strcmp(S.to, PEER), "selected contact provides the exact number"); on_ui(inspect_compose);
}
static void isolation(void) {
    CHECK(ink_member_switch("kid") == 0, "switch to synthetic child"); quiet(); open_list();
    CHECK(strstr(snap, "\"total\":0") && !strstr(snap, "合成短信"), "child cannot read adult SMS");
    CHECK(AI("data.list", "{\"collection\":\"sms.messages\"}") && g_rc.status == INK_RC_DONE && !strstr(g_result, "合成短信"), "AI data reads obey the same member boundary");
    CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "child may open only their own empty conversation"); quiet(); on_ui(snapshot_page);
    CHECK(strstr(snap, "\"total\":0"), "guessing an adult peer does not expose a record");
    CHECK(ink_member_switch("owner") == 0, "return to synthetic adult"); quiet(); open_list();
    CHECK(strstr(snap, "合成短信") || strstr(snap, "已发出") || S.count > 0, "adult retains their conversations");
}

static void incoming_long(void *arg) { (void)arg; CHECK(sim_net_deliver_sms(PEER, body), "AT model accepts long inbound SMS"); }
static bool long_event;
static void check_long_event(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event; char member[16], text[355];
    ink_args_get_str(a, "member", member, sizeof member); ink_args_get_str(a, "text", text, sizeof text);
    CHECK(!strcmp(member, "owner") && !strcmp(text, body), "long event retains both full body and member metadata"); long_event = true;
}
static bool failure_injected;
static void inject_failure(void *arg) { fail_modem(arg); failure_injected = true; }
static void check_receipt(void) {
    lv_area_t notice, button; lv_obj_update_layout(S.notice); lv_obj_get_coords(S.notice, &notice);
    CHECK(!strcmp(ink_ui_page_name(), "SmsThread") ? !strcmp(S.result, "已发出") && S.count > 0 : !strcmp(lv_label_get_text(S.notice), S.result), "final receipt is on the outgoing bubble or composer notice");
    wanted = "展开"; located = NULL; find_label(lv_screen_active());
    CHECK(located, "long editor button exists");
    if (located) {
        lv_obj_get_coords(lv_obj_get_parent(located), &button);
        CHECK(notice.y2 < button.y1, "final receipt stays above the button: %d..%d vs %d..%d",
            (int)notice.y1, (int)notice.y2, (int)button.y1, (int)button.y2);
    }
}
static char pieces[355];
static void inspect_piece(void) {
    const char *text = S.texts[0];
    snapshot_page(); CHECK(rendered_text(lv_screen_active(), text), "every visible page fits its real message card");
    CHECK(strlen(pieces) + strlen(text) < sizeof pieces, "page text stays within the original message");
    if (strlen(pieces) + strlen(text) < sizeof pieces) strcat(pieces, text);
}
static void check_long_record(void) {
    ink_db_t *db = ink_db_open_member(ink_db_find_def("sms.messages"), "owner");
    ink_db_cursor_t cur = {0}; ink_rec_t rec; char peer[24], text[355], status[96];
    CHECK(db && ink_db_count(db) == 1 && ink_db_next(db, &cur, &rec, NULL, 0) == 1 &&
        read_message(db, rec.id, peer, text, status) && !strcmp(text, body) && !strcmp(status, "收到"),
        "all long inbound bytes and receipt survive persistence"); ink_db_close(db);
}
static int fixture_count;
static bool measure_flush;
static lv_area_t flush_bounds;
static unsigned flush_count;
static size_t pages_before_send;
static lv_obj_t *thread_screen;
static char fixture_peer[24], shot_name[48], latest_text[1024];
static void measured(lv_event_t *e) {
    if (!measure_flush) return;
    const lv_area_t *a = lv_event_get_param(e);
    if (!flush_count++) flush_bounds = *a;
    else { flush_bounds.x1 = LV_MIN(flush_bounds.x1, a->x1); flush_bounds.y1 = LV_MIN(flush_bounds.y1, a->y1); flush_bounds.x2 = LV_MAX(flush_bounds.x2, a->x2); flush_bounds.y2 = LV_MAX(flush_bounds.y2, a->y2); }
}
static void start_measure(void) {
    flush_count = 0; pages_before_send = S.thread_pages; measure_flush = true; thread_screen = lv_screen_active();
    lv_display_add_event_cb(lv_display_get_default(), measured, LV_EVENT_FLUSH_START, NULL);
}
static void end_measure(void) {
    measure_flush = false;
    CHECK(thread_screen == lv_screen_active(), "inline send keeps its page instead of rebuilding the screen");
    int top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L + (pages_before_send == S.thread_pages ? INK_CONTROL + INK_GAP : 0);
    CHECK(flush_count && flush_bounds.y1 >= ink_ui_px(top), "send refresh excludes the header and only includes the pager when its page count changes: %d,%d..%d,%d (%u strips)", (int)flush_bounds.x1, (int)flush_bounds.y1, (int)flush_bounds.x2, (int)flush_bounds.y2, flush_count);
}
static void inspect_bubbles(void) {
    snapshot_page();
    CHECK(S.visible > 0 && S.visible <= SMS_VISIBLE, "bounded visible message count");
    CHECK(S.messages.field_count == S.visible, "semantic fields enumerate every visible bubble");
    int previous_y = ink_ui_px(INK_CANVAS_H);
    for (size_t i = 0; i < S.visible; i++) {
        wanted = S.texts[i]; located = NULL; find_label(S.thread_slot);
        CHECK(located, "each semantic body has a rendered label"); if (!located) continue;
        lv_area_t label, bubble, frame; lv_obj_update_layout(S.thread_slot); lv_obj_get_coords(located, &label); lv_obj_get_coords(lv_obj_get_parent(located), &bubble); lv_obj_get_coords(S.thread_slot, &frame);
        CHECK(bubble.y2 < previous_y && bubble.y1 >= frame.y1 && bubble.y2 <= frame.y2, "bubbles fit bottom-up without overlap"); previous_y = bubble.y1;
        CHECK(label.x1 >= bubble.x1 && label.x2 <= bubble.x2 && label.y1 >= bubble.y1 && label.y2 <= bubble.y2, "every complete body fits inside its bubble");
        CHECK(S.outgoing[i] ? bubble.x2 == ink_ui_px(INK_CANVAS_W - INK_MARGIN) - 1 : bubble.x1 == ink_ui_px(INK_MARGIN), "incoming and outgoing keep their own alignment");
        CHECK(!strcmp(S.message_fields[S.visible - 1 - i].value, S.texts[i]) && !strcmp(S.message_fields[S.visible - 1 - i].label, S.labels[i]), "snapshot order and metadata match the visible bubble");
        if (S.outgoing[i]) {
            lv_obj_t *state_label = lv_obj_get_child(S.thread_slot, lv_obj_get_index(lv_obj_get_parent(located)) + 1);
            CHECK(state_label && lv_obj_check_type(state_label, &lv_label_class) && !strcmp(lv_label_get_text(state_label), S.states[i]), "outgoing state is rendered separately");
            if (state_label) { lv_area_t state; lv_obj_get_coords(state_label, &state); CHECK(state.y1 > bubble.y2, "outgoing state is below its own bubble"); }
        }
    }
    size_t expected_stamps = 0, actual_stamps = 0;
    for (size_t i = 0; i < S.visible; i++) if (S.stamps[i] || i + 1 == S.visible) expected_stamps++;
    for (uint32_t i = 0; i < lv_obj_get_child_count(S.thread_slot); i++) {
        lv_obj_t *o = lv_obj_get_child(S.thread_slot, i);
        if (lv_obj_check_type(o, &lv_label_class)) { const char *v = lv_label_get_text(o); if (strlen(v) == 16 && v[4] == '-' && v[7] == '-') actual_stamps++; }
    }
    CHECK(actual_stamps == expected_stamps, "only the first bubble in each visible time group has a timestamp");
    lv_area_t bar, frame; lv_obj_get_coords(lv_obj_get_child(lv_obj_get_parent(S.thread_slot), -1), &bar); lv_obj_get_coords(S.thread_slot, &frame);
    CHECK(bar.y2 < frame.y1, "paging buttons stay above all messages");
}
static void export_thread(void *arg) {
    (void)arg; char path[768]; int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, shot_name);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb"); if (!f || fputs(snap, f) < 0 || fclose(f)) abort();
    n = snprintf(path, sizeof path, "%s/refresh.txt", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    f = fopen(path, "wb"); if (!f || fprintf(f, "%d,%d..%d,%d; strips=%u\n", (int)flush_bounds.x1, (int)flush_bounds.y1, (int)flush_bounds.x2, (int)flush_bounds.y2, flush_count) < 0 || fclose(f)) abort();
}
static void thread_capture(void *arg) { (void)arg; CHECK(settled(), "thread screenshot is settled"); sim_display_snapshot(shot_name); }
static void thread_scene(const char *suffix) {
    quiet(); on_ui(inspect_bubbles); snprintf(shot_name, sizeof shot_name, "thread-%d%s", fixture_count, suffix);
    vtk_host_call(export_thread, NULL); vtk_event_at(vtk_now() + SIM_MS(1), thread_capture, NULL); delay_ms(5);
}
static void thread_acceptance(int total, bool large) {
    fixture_count = total;
    if (large) { CHECK(BY_HAND("settings.set", "{\"id\":\"display.font_size\",\"value\":3}") && g_rc.status == INK_RC_DONE, "use real 120 percent text size"); quiet(); }
    snprintf(fixture_peer, sizeof fixture_peer, "000000001%02d", total);
    ink_db_t *db = ink_db_open_member(ink_db_find_def("sms.messages"), "owner"); CHECK(db, "synthetic thread uses the member store");
    int64_t now = ink_time_now_s();
    for (int i = 1; db && i <= total; i++) {
        char text[355], status[96], raw[768]; bool own = ((i - 1) / 2) % 2 != 0;
        snprintf(text, sizeof text, "合成%d：%s", i, own ? "好的" : "下午见");
        if (total == 20 && i == 16) { text[0] = 0; for (int line = 0; line < 16; line++) strcat(text, line ? "\n合成长短信" : "合成长短信"); }
        snprintf(status, sizeof status, "%s", i == total - 1 ? "发送失败，未发出" : i == total - 2 && total > 5 ? "结果未知，请先核实" : own ? "已发出" : "收到");
        if (total == 20 && i == 19) ink_time_set(now + 301, false);
        ink_jw_t w; ink_jw_init(&w, raw, sizeof raw); ink_jw_raw(&w, "{\"peer\":"); ink_jw_str(&w, fixture_peer); ink_jw_raw(&w, ",\"text\":"); ink_jw_str(&w, text); ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, status); ink_jw_raw(&w, "}");
        ink_db_write_t wr = {.sort = i, .data = raw, .len = w.len, .src = INK_SRC_SYSTEM};
        CHECK(!w.overflow && ink_db_put(db, &wr, NULL) == INK_DB_OK, "insert synthetic thread fixture %d", i);
    }
    ink_db_close(db); snprintf(arguments, sizeof arguments, "{\"peer\":\"%s\"}", fixture_peer);
    CHECK(AI("sms.thread", arguments) && g_rc.status == INK_RC_DONE, "open synthetic conversation through the shared capability"); quiet();
    CHECK(S.count == (size_t)total && S.position.page + 1 == S.thread_pages, "thread opens on its latest page");
    if (total == 20) CHECK(S.groups[1] && !S.groups[0], "a gap beyond five minutes starts a new outgoing time group");
    if (total > 1) CHECK(S.visible > 1, "one page holds multiple actual bubbles");
    snprintf(latest_text, sizeof latest_text, "%s", S.texts[0]); thread_scene(large ? "-large" : "");
    if (S.thread_pages > 1) {
        size_t latest = S.position.page;
        CHECK(AI("sms.prev", NULL) && g_rc.status == INK_RC_DONE, "AI opens the earlier page"); quiet();
        CHECK(S.position.page + 1 == latest && strcmp(S.texts[0], latest_text), "earlier page shows older messages"); thread_scene(large ? "-large-older" : "-older");
        if (total == 20 && S.position.page) { CHECK(AI("sms.prev", NULL) && g_rc.status == INK_RC_DONE, "read the full long fragment"); quiet(); thread_scene(large ? "-large-long" : "-long"); CHECK(AI("sms.next", NULL) && g_rc.status == INK_RC_DONE, "return from the long fragment"); quiet(); }
        CHECK(AI("sms.next", NULL) && g_rc.status == INK_RC_DONE, "AI returns to the newer page"); quiet();
        CHECK(S.position.page == latest && !strcmp(S.texts[0], latest_text), "return preserves the newest visible body");
    }
    if (total == 5) {
        size_t failed = SMS_VISIBLE;
        for (size_t i = 0; i < S.visible; i++) if (strstr(S.states[i], "发送失败，未发出")) failed = i;
        CHECK(failed < S.visible, "failure remains available on the latest page");
        if (failed < S.visible) { CHECK(AI("sms.retry", S.args[failed]) && g_rc.status == INK_RC_DONE, "AI uses the same retry capability"); quiet(); CHECK(!strcmp(ink_ui_page_name(), "SmsCompose") && !strcmp(S.to, fixture_peer) && strstr(ink_textfield_text(S.field), "合成4"), "retry retains the exact failed body and peer"); int before = sim_net_sms_sent(); tap_text("发送"); CHECK(WAIT_FOR(sim_net_sms_sent() == before + 1 && !strcmp(ink_ui_page_name(), "SmsThread"), 10000), "deliberate retry sends once"); quiet(); }
    }
    CHECK(AI("sms.text", NULL) && g_rc.status == INK_RC_DONE, "focus inline reply"); on_ui(type_short); quiet();
    int before = sim_net_sms_sent(); size_t before_count = S.count; on_ui(start_measure); tap_text("发送");
    CHECK(WAIT_FOR(sim_net_sms_sent() == before + 1 && S.count == before_count + 1 && S.result[0] && settled(), 10000), "inline send finishes once"); quiet(); on_ui(end_measure);
    snprintf(shot_name, sizeof shot_name, "refresh"); on_ui(snapshot_page); vtk_host_call(export_thread, NULL);
}
static bool review_phase(void) {
    if (!strcmp(phase, "legacy-send")) {
        CHECK(ink_cap_find("message.send")->run == ink_cap_find("sms.send")->run, "both public send names share one implementation");
        CHECK(BY_HAND("message.send", "{\"to\":\"00000000001\",\"text\":\"合成短信\"}") && g_rc.status == INK_RC_DONE,
            "legacy send succeeds through AT");
        CHECK(sim_net_sms_sent() == 1 && stored("owner") == 1 && has_status("已发出"), "legacy send is durably recorded once");
    } else if (!strcmp(phase, "long-rx") || !strcmp(phase, "long-rx-read")) {
        if (!strcmp(phase, "long-rx")) {
            CHECK(ink_cap_subscribe("message.received", check_long_event, NULL, INK_EXEC_BG) == 0, "observe the real long incoming event");
            vtk_event_at(vtk_now() + SIM_MS(1), incoming_long, NULL);
            CHECK(WAIT_FOR(stored("owner") == 1 && long_event, 10000), "long inbound reaches the member store");
        }
        check_long_record();
    } else if (!strcmp(phase, "cold-thread")) {
        deliver(); CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "cold AI thread navigation succeeds");
        CHECK(WAIT_FOR(S.list != NULL, 10000), "cold thread loads"); quiet();
        CHECK(!strcmp(S.peer, PEER) && S.count == 1, "first navigation preserves the requested peer and records");
        on_ui(snapshot_page); CHECK(strstr(snap, "合成短信"), "cold thread snapshot contains the incoming message");
    } else if (!strcmp(phase, "multiline")) {
        body[0] = 0; for (int i = 0; i < 16; i++) strcat(body, i ? "\n中中中中中" : "中中中中中");
        ink_jw_t w; ink_jw_init(&w, arguments, sizeof arguments); ink_jw_raw(&w, "{\"to\":\"00000000001\",\"text\":"); ink_jw_str(&w, body); ink_jw_raw(&w, "}");
        CHECK(BY_HAND("message.send", arguments) && g_rc.status == INK_RC_DONE, "send valid multiline SMS");
        CHECK(sim_net_sent_sms(0, sent_number, sizeof sent_number, sent_text, sizeof sent_text) == 1 && !strcmp(sent_text, body), "AT receives all multiline bytes");
        CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "open multiline thread"); quiet();
        char peer[24], text[355], status[96]; CHECK(S.count == 1 && read_message(S.db, S.rows[0], peer, text, status) && !strcmp(text, body), "multiline SMS is stored unchanged");
        size_t pages = S.thread_pages; CHECK(pages > 1, "multiline body exposes continuation pages");
        CHECK(S.position.page + 1 == pages, "long thread opens at the newest fragment");
        while (S.position.page) { CHECK(AI("sms.prev", NULL) && g_rc.status == INK_RC_DONE, "go to the first long fragment"); quiet(); }
        for (size_t i = 0; i < pages; i++) { on_ui(inspect_piece); if (i + 1 < pages) { CHECK(AI("sms.next", NULL) && g_rc.status == INK_RC_DONE, "AI turns to the next body page"); quiet(); } }
        CHECK(!strcmp(pieces, body), "visible pages reconstruct the exact multiline SMS");
        CHECK(AI("sms.prev", NULL) && g_rc.status == INK_RC_DONE && S.position.page + 2 == pages, "previous page returns to earlier text");
    } else if (!strncmp(phase, "receipt-", 8)) {
        open_list(); compose(); CHECK(AI("sms.text", NULL) && g_rc.status == INK_RC_DONE, "focus short editor"); on_ui(type_short);
        if (!strcmp(phase, "receipt-fail")) { on_ui(no_registration); CHECK(WAIT_FOR(!sim_net_modem_registered(), 10000), "modem is actually unregistered"); }
        if (!strcmp(phase, "receipt-unknown")) { vtk_event_at(vtk_now() + SIM_MS(1), inject_failure, NULL); CHECK(WAIT_FOR(failure_injected, 10000), "modem failure applied"); }
        tap_text("发送"); CHECK(WAIT_FOR(S.result[0] && settled(), 10000), "final UI receipt arrives");
        CHECK(!strcmp(S.result, !strcmp(phase, "receipt-ok") ? "已发出" : !strcmp(phase, "receipt-fail") ? "发送失败，未发出" : "结果未知，请先核实"), "UI reports the actual send outcome"); on_ui(check_receipt);
    } else if (!strcmp(phase, "draft-write") || !strcmp(phase, "draft-read")) {
        ink_db_t *db = ink_db_open_member(ink_db_find_def("text.drafts"), "owner");
        if (!strcmp(phase, "draft-read")) CHECK(db && ink_db_count(db) == 1, "saved draft exists after process restart");
        ink_db_close(db);
        open_list(); compose(); CHECK(AI("sms.long", NULL) && g_rc.status == INK_RC_DONE, "open or resume F3");
        if (!strcmp(phase, "draft-write")) on_ui(type_long);
        on_ui(inspect_long); CHECK(WAIT_FOR(ink_text_draft_saved() == 1, 10000), "F3 reports durable draft");
        db = ink_db_open_member(ink_db_find_def("text.drafts"), "owner"); CHECK(db && ink_db_count(db) == 1, "resume reuses the original draft reference"); ink_db_close(db);
    } else if (!strncmp(phase, "thread-", 7)) { thread_acceptance(atoi(phase + 7), strstr(phase, "large") != NULL); return true; }
    else return false;
    quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture, NULL); return true;
}
static void run(void *arg) {
    (void)arg; vtk_host_call(read_phase, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    CHECK(WAIT_FOR(sim_net_modem_registered() && registered(), 10000), "simulated AT modem publishes registration"); quiet();
    for (int i = 0; i < 118; i++) memcpy(body + i * 3, "中", 3);
    body[354] = 0;
    if (review_phase()) { /* Covered above, with isolated flash images. */
    } else if (!strcmp(phase, "legacy")) {
        open_list(); deliver(); CHECK(ink_member_switch("kid") == 0, "probe the original SMS capability after handoff"); quiet();
        CHECK(AI("message.list", NULL) && g_rc.status == INK_RC_DONE && !strstr(g_rc.say, "合成短信"), "legacy message.list must not expose the adult inbox");
    } else if (!strcmp(phase, "verify")) {
        open_list(); CHECK(stored("owner") == 21 && has_status("收到") && has_status("已发出") && has_status("发送失败，未发出") && has_status("结果未知，请先核实"), "inbound history beyond 16 and all outgoing results survive a fresh process"); isolation();
    } else if (!strcmp(phase, "SmsCompose")) {
        open_list(); compose();
        CHECK(AI("sms.text", NULL) && g_rc.status == INK_RC_DONE, "focus compose capture");
        CHECK(AI("text.insert", "{\"text\":\"今天\"}") && g_rc.status == INK_RC_DONE, "type only a synthetic screenshot fixture"); scene("SmsCompose");
    }
    else if (!strcmp(phase, "SmsThread")) {
        open_list(); snprintf(arguments, sizeof arguments, "{\"to\":\"%s\",\"text\":\"%s\"}", PEER, body);
        CHECK(BY_HAND("sms.send", arguments) && g_rc.status == INK_RC_DONE, "send long fixture through the AT model");
        CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "open the long conversation"); quiet(); on_ui(inspect_thread); scene("SmsThread"); thread_acceptance(20, false);
    } else {
        open_list(); CHECK(stored("owner") == 0, "fresh SMS page has no invented data");
        for (int i = 0; i < 17; i++) deliver();
        open_list(); CHECK(S.count == 1, "17 incoming messages group into one peer conversation"); scene("Sms");
        compose(); CHECK(AI("sms.text", NULL) && g_rc.status == INK_RC_DONE, "focus the F2 short host"); on_ui(type_short);
        CHECK(AI("sms.long", NULL) && g_rc.status == INK_RC_DONE, "open F3 long editor");
        on_ui(type_long);
        on_ui(inspect_long);
        CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "return long text to composer"); quiet(); on_ui(inspect_compose);
        int count = stored("owner"); tap_text("发送"); CHECK(WAIT_FOR(stored("owner") == count + 1 && has_status("已发出") && durable(), 10000), "physical send button persists and sends once");
        CHECK(sim_net_sent_sms(0, sent_number, sizeof sent_number, sent_text, sizeof sent_text) == 1 && !strcmp(sent_text, body) && !strcmp(sent_number, PEER), "118 Chinese characters arrive unchanged at the simulated AT boundary");
        CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "open long message"); quiet(); on_ui(inspect_thread);
        snprintf(arguments, sizeof arguments, "{\"to\":\"%s\",\"text\":\"合成回执\"}", PEER);
        on_ui(no_registration); CHECK(WAIT_FOR(!sim_net_modem_registered(), 10000), "remove the simulated SIM before sending");
        CHECK(BY_HAND("sms.send", arguments) && g_rc.status != INK_RC_DONE && has_status("发送失败，未发出"), "known preflight failure is persisted as not sent"); on_ui(restore_registration);
        CHECK(WAIT_FOR(sim_net_modem_registered(), 10000), "restored simulated SIM registers again");
        vtk_event_at(vtk_now() + SIM_MS(1), fail_modem, NULL); delay_ms(5);
        CHECK(BY_HAND("sms.send", arguments) && g_rc.status == INK_RC_FAILED && has_status("结果未知，请先核实"), "undifferentiated modem error is honestly unknown");
        vtk_event_at(vtk_now() + SIM_MS(1), restore_modem, NULL); delay_ms(5);
        CHECK(ink_cap_subscribe("confirm.result", confirmed, NULL, INK_EXEC_DIRECT) == 0, "observe actual confirmation receipts");
        int before = sim_net_sms_sent(); CHECK(AI("sms.send", arguments) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "unsolicited AI send requires confirmation");
        ink_confirm_view_t pending[1]; CHECK(ink_confirm_list(pending, 1) == 1 && sim_net_sms_sent() == before, "confirmation holds exact external effect");
        confirmation_done = false; CHECK(ink_confirm_decide(pending[0].token, true, INK_SRC_TOUCH) == 0, "human accepts the send card");
        CHECK(WAIT_FOR(confirmation_done, 10000) && !strcmp(confirmation_status, "done") && sim_net_sms_sent() == before + 1, "confirmed AI send uses the same durable provider once");
        isolation(); CHECK(stored("owner") == 21, "adult retains every received and sent record");
    }
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000) && durable(), "all SMS records are durable before ending"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); osal_task_cfg_t cfg = {.name = "sms-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start bounded SMS acceptance test");
}
