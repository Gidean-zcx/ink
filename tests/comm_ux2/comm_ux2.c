/* Public UI, capabilities and synthetic modem only; also linkable against the baseline. */
#include <stdlib.h>
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "../pagebar.h"
#include "ink/sys/appreg.h"
#include "ink/sys/textinput.h"
#include "ink/sys/notify.h"
#include "ink/ui_theme.h"
#include "ink/ui_shell.h"

static char snap[6144], number[32], text[256], arguments[384], mode[24], shot[40];
static const char *wanted;
static lv_obj_t *found;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "UI settles"); }
static void read_mode(void *arg) { (void)arg; const char *m = getenv("INK_COMM_UX_MODE"); snprintf(mode, sizeof mode, "%s", m ? m : ""); }
static void read_page(void) { CHECK(ink_ui_snapshot(snap, sizeof snap, NULL) > 0, "page has a current AI snapshot"); CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "page fits the production AI context"); }
static void inspect(void) { on_ui(read_page); }
static void find_label(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) found = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, i));
}
static void locate(void) {
    found = NULL; find_label(lv_screen_active()); CHECK(found, "visible control %s", wanted);
    if (!found) return;
    lv_obj_t *o = lv_obj_get_parent(found); lv_obj_update_layout(o); lv_area_t a; lv_obj_get_coords(o, &a);
    CHECK(lv_area_get_height(&a) >= ink_ui_px(INK_CONTROL), "full touch target for %s", wanted);
    g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2;
}
static void click(const char *label) { wanted = label; on_ui(locate); if (found) tap(g_btn_x[0], g_btn_y[0]); quiet(); }
static void export_page(void *arg) {
    (void)arg; char path[768]; int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, shot);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb"); if (!f || fputs(snap, f) < 0 || fclose(f)) abort();
}
static void locate_app(void) {
    found = NULL; wanted = "联系人"; find_label(lv_screen_active()); CHECK(found, "contacts appears in the app list");
    if (!found) return;
    lv_obj_t *button = lv_obj_get_child(lv_obj_get_parent(found), lv_obj_get_index(found) - 1);
    lv_area_t a; lv_obj_update_layout(button); lv_obj_get_coords(button, &a);
    CHECK(lv_area_get_height(&a) >= ink_ui_px(INK_CONTROL), "app icon keeps the standard touch target");
    g_btn_x[0] = (a.x1 + a.x2) / 2; g_btn_y[0] = (a.y1 + a.y2) / 2;
}
static void capture(void *arg) { (void)arg; CHECK(settled(), "capture is settled"); sim_display_snapshot(shot); }
static void scene(const char *name) { quiet(); inspect(); snprintf(shot, sizeof shot, "%s", name); vtk_host_call(export_page, NULL); vtk_event_at(vtk_now() + SIM_MS(1), capture, NULL); delay_ms(5); }
static void replace_input(void) { ink_text_select(0, ink_text_char_offset(ink_text_value(), strlen(ink_text_value()))); }
static void finish_draft(void) { ink_text_blur(0); }
static void enter(const char *cap, const char *value) {
    CHECK(AI(cap, NULL) && g_rc.status == INK_RC_DONE, "focus %s", cap); quiet(); on_ui(replace_input);
    snprintf(arguments, sizeof arguments, "{\"text\":\"%s\"}", value);
    CHECK(AI("text.insert", arguments) && g_rc.status == INK_RC_DONE, "edit through shared input");
    if (!strcmp(cap, "sms.text")) on_ui(finish_draft);
    else CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_DONE, "submit shared input");
    quiet();
}
static void normalization(void) {
    int before = sim_net_sms_sent();
    CHECK(BY_HAND("message.send", "{\"to\":\"+00 (000)-000 002\",\"text\":\"合成格式号码测试\"}") && g_rc.status == INK_RC_DONE, "formatted SMS is accepted");
    CHECK(sim_net_sms_sent() == before + 1 && sim_net_sent_sms(before, number, sizeof number, text, sizeof text) == before + 1 && !strcmp(number, "+00000000002"), "SMS reaches modem with normalized number");
    CHECK(BY_HAND("call.dial", "{\"number\":\"00 (000)-000 003\"}") && g_rc.status == INK_RC_DONE, "formatted dial is accepted");
    CHECK(WAIT_FOR(sim_net_call(number, sizeof number) == 1, 10000) && !strcmp(number, "00000000003"), "call reaches modem with normalized number");
    CHECK(BY_HAND("call.hangup", NULL) && g_rc.status == INK_RC_DONE, "end synthetic call"); quiet();
    const char *bad[] = {"+", "--()", "12a3", "12+3"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        snprintf(arguments, sizeof arguments, "{\"to\":\"%s\",\"text\":\"合成无效号码\"}", bad[i]);
        CHECK(BY_HAND("sms.send", arguments) && g_rc.status == INK_RC_INVALID, "reject invalid formatted SMS %u", i);
    }
}
static void contact_footer(void) {
    wanted = "添加联系人"; found = NULL; find_label(lv_screen_active()); CHECK(found, "contact add button is visible");
    if (!found) return;
    lv_area_t add, bar; lv_obj_update_layout(lv_screen_active()); lv_obj_get_coords(lv_obj_get_parent(found), &add);
    found = ink_test_pagebar(lv_screen_active(), 1, 1); CHECK(found, "contact page bar has one selected dot");
    if (!found) return;
    lv_obj_get_coords(found, &bar);
    CHECK(add.y1 > bar.y2 + ink_ui_px(INK_GAP), "page bar and add button keep separate full touch targets");
}
static void contacts(void) {
    CHECK(AI("data.list", "{\"collection\":\"phone.contacts\"}") && g_rc.status == INK_RC_DONE, "load empty member contacts");
    CHECK(BY_HAND("system.home", NULL) && BY_HAND("home.next", NULL), "open the full app list"); quiet(); scene("app-list");
    on_ui(locate_app); if (found) tap(g_btn_x[0], g_btn_y[0]); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "PhoneContacts"), "step 1: app-list touch opens independent contacts");
    click("添加联系人"); CHECK(!strcmp(ink_ui_page_name(), "ContactEdit"), "step 2: new contact form"); scene("contacts-new");
    enter("contacts.name", "Z合成联系人"); enter("contacts.number", "00000000001"); enter("contacts.note", "合成备注");
    click("保存联系人"); CHECK(!strcmp(ink_ui_page_name(), "PhoneContacts"), "save returns to contact list");
    CHECK(AI("contacts.put", "{\"name\":\"A合成联系人\",\"number\":\"00000000004\"}") && g_rc.status == INK_RC_DONE, "second contact for name ordering"); quiet(); inspect();
    const char *a = strstr(snap, "A合成联系人"), *z = strstr(snap, "Z合成联系人"); CHECK(a && z && a < z, "contacts sort by name"); on_ui(contact_footer); scene("contacts-list");
    enter("contacts.search", "Z合成"); inspect(); CHECK(strstr(snap, "\"total\":1") && !strstr(snap, "A合成联系人"), "search filters the real list");
    click("Z合成联系人"); inspect(); CHECK(!strcmp(ink_ui_page_name(), "ContactDetail") && strstr(snap, "合成备注") && strstr(snap, "call.dial") && strstr(snap, "sms.compose"), "detail offers calling and messaging with persisted notes"); scene("contact-detail");
    click("发短信"); inspect(); CHECK(!strcmp(ink_ui_page_name(), "SmsCompose") && strstr(snap, "00000000001"), "contact detail fills the SMS recipient");
    CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "leave contact composer before the unlisted-number flow"); quiet();
}
static void incoming(void *arg) { (void)arg; CHECK(sim_net_deliver_sms("00000000001", "合成来信"), "deliver synthetic incoming SMS"); }
static void no_card(void *arg) { (void)arg; sim_net_set_card(0, false, false, false); }
static void restore_card(void *arg) { (void)arg; sim_net_set_card(0, true, true, true); sim_net_modem_power(false); sim_net_modem_power(true); }
static void sms(void) {
    CHECK(BY_HAND("shell.launch", "{\"id\":\"message\"}") && g_rc.status == INK_RC_DONE, "launch SMS"); quiet();
    click("新建"); inspect(); CHECK(strstr(snap, "sms.to") && strstr(snap, "sms.pick") && !strstr(snap, "message.send") && ink_cap_find("message.send"), "new composer exposes recipient, picker and send");
    enter("sms.to", "00 (000)-000 002"); scene("sms-new-number"); enter("sms.text", "合成陌生号码短信");
    int before = sim_net_sms_sent(); click("发送");
    CHECK(WAIT_FOR(sim_net_sms_sent() == before + 1 && !strcmp(ink_ui_page_name(), "SmsThread"), 10000), "three-step send opens its conversation"); quiet();
    CHECK(sim_net_sent_sms(before, number, sizeof number, text, sizeof text) == before + 1 && !strcmp(number, "00000000002") && !strcmp(text, "合成陌生号码短信"), "unlisted formatted recipient and full body reach modem");
    inspect(); CHECK(strstr(snap, "已发出") && strstr(snap, "sms.text") && !strstr(snap, "message.send") && ink_cap_find("message.send"), "thread has receipt and inline reply"); scene("sms-thread");
    CHECK(BY_HAND("sms.compose", NULL) && g_rc.status == INK_RC_DONE, "new composer for picker"); quiet(); enter("sms.text", "合成选择前草稿"); click("+");
    CHECK(!strcmp(ink_ui_page_name(), "PhoneContacts"), "plus opens contact picker"); quiet(); click("A合成联系人");
    CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "SmsCompose"), 10000), "picker returns to draft"); quiet(); inspect(); CHECK(strstr(snap, "00000000004") && strstr(snap, "合成选择前草稿"), "picker supplies selected number and retains the draft");
    CHECK(BY_HAND("sms.compose", NULL) && g_rc.status == INK_RC_DONE, "empty composer for matching"); quiet(); enter("sms.to", "Z合成"); inspect(); CHECK(strstr(snap, "Z合成联系人") && strstr(snap, "sms.recipient"), "recipient entry suggests a matching contact"); click("Z合成联系人"); inspect(); CHECK(strstr(snap, "00000000001"), "matching suggestion selects exact recipient");
    vtk_event_at(vtk_now() + SIM_MS(1), no_card, NULL); CHECK(WAIT_FOR(!sim_net_modem_registered(), 10000), "synthetic modem is offline");
    CHECK(BY_HAND("sms.send", "{\"to\":\"00000000001\",\"text\":\"合成失败重发\"}") && g_rc.status == INK_RC_FAILED, "known unsent failure is persisted");
    CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "open failed message thread"); quiet(); scene("sms-failed");
    click("合成失败重发"); CHECK(!strcmp(ink_ui_page_name(), "SmsCompose"), "failed message opens for retry"); inspect(); CHECK(strstr(snap, "合成失败重发"), "retry preserves body and recipient");
    vtk_event_at(vtk_now() + SIM_MS(1), restore_card, NULL); CHECK(WAIT_FOR(sim_net_modem_registered(), 10000), "synthetic modem returns"); quiet(); before = sim_net_sms_sent(); click("发送"); CHECK(WAIT_FOR(sim_net_sms_sent() == before + 1, 10000), "retry sends once after a deliberate touch"); quiet();
    vtk_event_at(vtk_now() + SIM_MS(1), incoming, NULL); CHECK(WAIT_FOR(ink_notify_unread() > 0, 10000), "incoming SMS creates unread notification"); quiet();
    CHECK(BY_HAND("shell.back", NULL) && g_rc.status == INK_RC_DONE, "return to conversation list"); quiet(); inspect(); CHECK(strstr(snap, "Z合成联系人") && strstr(snap, "合成来信") && strstr(snap, "●"), "conversation row has name, last text and unread marker"); scene("sms-list");
    CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "open unread conversation"); CHECK(WAIT_FOR(ink_notify_unread() == 0, 10000), "opening conversation marks its notifications read");
}
static void phone(void) {
    CHECK(AI("phone.open", "{\"number\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "open dialler with a matching number"); quiet(); inspect(); CHECK(strstr(snap, "Z合成联系人") && strstr(snap, "phone.contacts") && strstr(snap, "phone.recents"), "dialler exposes matching contact and three entries"); scene("phone-match");
    CHECK(AI("phone.contacts", NULL) && g_rc.status == INK_RC_DONE, "contact segment opens contacts"); quiet(); scene("phone-contacts");
    CHECK(ink_member_switch("kid") == 0, "switch to child"); quiet();
    CHECK(BY_HAND("shell.launch", "{\"id\":\"contacts\"}") && g_rc.status == INK_RC_DONE, "child can launch own contacts"); quiet(); inspect(); CHECK(!strstr(snap, "合成联系人") && !strstr(snap, "合成备注"), "child cannot read adult contact fields or search");
}
static void run(void *arg) {
    (void)arg; vtk_host_call(read_mode, NULL);
    CHECK(WAIT_FOR(ink_member_space_open() && sim_net_modem_registered(), 10000), "member and modem ready"); quiet();
    if (!strcmp(mode, "baseline")) {
        bool listed = false; for (int i = 0; i < ink_app_count(); i++) if (!strcmp(ink_app_at(i)->id, "contacts")) listed = true;
        CHECK(listed, "independent contacts entry exists");
        CHECK(BY_HAND("system.home", NULL) && BY_HAND("home.next", NULL), "baseline app list"); quiet(); scene("before-app-list");
        CHECK(AI("data.list", "{\"collection\":\"sms.messages\"}") && AI("data.list", "{\"collection\":\"phone.contacts\"}"), "warm baseline collections");
        CHECK(BY_HAND("sms.compose", NULL) && g_rc.status == INK_RC_DONE, "open empty composer"); quiet(); inspect(); CHECK(strstr(snap, "sms.to") && ink_cap_find("message.send"), "empty composer accepts an unlisted recipient"); scene("before-sms-new");
        CHECK(BY_HAND("contacts.edit", NULL) && g_rc.status == INK_RC_DONE, "baseline contact form"); quiet(); scene("before-contacts-new");
        normalization();
    } else { contacts(); sms(); phone(); normalization(); }
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "writes reach persistence"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); osal_task_cfg_t cfg = {.name = "comm-ux-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start communication UX acceptance driver");
}
