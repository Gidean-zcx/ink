/* Long text messages through the real product and the PDU-mode modem model (3GPP TS 23.040 §9.2.3.24.1): parts put
 * together in any order, once; a missing part shown as incomplete after the wait; parts kept across a restart;
 * long messages sent in parts, with an honest receipt when one fails; the composer says how many parts. */
#include <stdlib.h>
#include "ink/sys/data.h"
#include "../../system/apps/sms/sms.c"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char phase[24], text[1200], got[1200], pdu[400], arguments[4096];
static struct { char num[24], text[1200]; int ref, total, seq; bool ref16, whole; } inj;

static void read_phase(void *arg) { (void)arg; snprintf(phase, sizeof phase, "%s", getenv("INK_SMS_PHASE")); }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static void inject(void *arg) {
    (void)arg;
    bool ok = inj.whole ? sim_net_deliver_sms(inj.num, inj.text) : sim_net_deliver_sms_part(inj.num, inj.text, inj.ref, inj.total, inj.seq, inj.ref16);
    CHECK(ok, "the network hands the device %s", inj.whole ? "a long message" : "a part");
}
static void part(const char *num, const char *t, int ref, int total, int seq, bool ref16) {
    snprintf(inj.num, sizeof inj.num, "%s", num); snprintf(inj.text, sizeof inj.text, "%s", t);
    inj.ref = ref, inj.total = total, inj.seq = seq, inj.ref16 = ref16, inj.whole = false;
    vtk_event_at(vtk_now() + SIM_MS(1), inject, NULL); delay_ms(400);
}
static void whole(const char *num, const char *t) {
    snprintf(inj.num, sizeof inj.num, "%s", num); snprintf(inj.text, sizeof inj.text, "%s", t); inj.whole = true;
    vtk_event_at(vtk_now() + SIM_MS(1), inject, NULL); delay_ms(400);
}
static int fail_nth;
static void fail_part(void *arg) { (void)arg; sim_net_sms_fail_part(fail_nth); }

/* Messages from (or to) peer in the owner's store, the newest one's text and status into got / status. */
static int messages(const char *peer, char *status) {
    ink_db_t *db = ink_db_open_member(ink_db_find_def("sms.messages"), "owner");
    ink_db_cursor_t cur = {0}; ink_rec_t rec; char p[24], t[INK_SMS_TEXT_MAX], st[96]; int n = 0;
    while (db && ink_db_next(db, &cur, &rec, NULL, 0) == 1)
        if (read_message(db, rec.id, p, t, st) && !strcmp(p, peer)) { n++; snprintf(got, sizeof got, "%s", t); if (status) snprintf(status, 96, "%s", st); }
    ink_db_close(db);
    return n;
}
static int kept_parts(void) {
    ink_db_t *db = ink_db_open(ink_db_find_def("sms.parts"), NULL);
    int n = db ? ink_db_count(db) : -1; ink_db_close(db); return n;
}
static bool parts_durable(void) {
    ink_db_t *db = ink_db_open(ink_db_find_def("sms.parts"), NULL);
    bool d = db && ink_db_durable_ver(db) == ink_db_last_ver(db); ink_db_close(db); return d;
}
static void han(char *out, int n, const char *c) { out[0] = 0; for (int i = 0; i < n; i++) strcat(out, c); }
static void capture_incomplete(void *arg) { (void)arg; sim_display_snapshot("incomplete"); }
static void capture_parts(void *arg) { (void)arg; sim_display_snapshot("compose-parts"); }

static void receive(void) {
    const char *A = "00000000011", *B = "00000000012", *C = "00000000013", *D = "00000000014", *E = "00000000015", *F = "00000000016";
    part(A, "甲段内容，", 7, 3, 1, false); part(A, "乙段内容，", 7, 3, 2, false); part(A, "丙段结束。", 7, 3, 3, false);
    CHECK(WAIT_FOR(messages(A, NULL) == 1, 5000) && !strcmp(got, "甲段内容，乙段内容，丙段结束。") && kept_parts() == 0,
          "3 parts in order are one message (%.150s)", got);
    part(B, "丙段结束。", 8, 3, 3, false); part(B, "甲段内容，", 8, 3, 1, false);
    CHECK(WAIT_FOR(kept_parts() == 2, 5000) && messages(B, NULL) == 0, "nothing is shown while a part is missing");
    part(B, "乙段内容，", 8, 3, 2, false);
    CHECK(WAIT_FOR(messages(B, NULL) == 1, 5000) && !strcmp(got, "甲段内容，乙段内容，丙段结束。"), "out of order: put in order (%.150s)", got);
    part(C, "甲段内容，", 9, 3, 1, false); part(C, "乙段内容，", 9, 3, 2, false); part(C, "乙段内容，", 9, 3, 2, false);
    CHECK(kept_parts() == 2, "a part that comes twice is kept once (%d)", kept_parts());
    part(C, "丙段结束。", 9, 3, 3, false); delay_ms(2000);
    CHECK(messages(C, NULL) == 1 && !strcmp(got, "甲段内容，乙段内容，丙段结束。") && kept_parts() == 0, "and appears once (%.150s)", got);
    part(D, "and GSM 7-bit with a 16-bit reference.", 0x1234, 2, 2, true); part(D, "Part one, @ 5 EUR ", 0x1234, 2, 1, true);
    CHECK(WAIT_FOR(messages(D, NULL) == 1, 5000) && !strcmp(got, "Part one, @ 5 EUR and GSM 7-bit with a 16-bit reference."), "GSM 7-bit parts (%.150s)", got);
    han(text, 300, "长"); whole(E, text);
    CHECK(WAIT_FOR(messages(E, NULL) == 1, 10000) && !strcmp(got, text), "300 Chinese characters split by the network come back whole");
    part(F, "开头一段，", 10, 3, 1, false); part(F, "最后一段。", 10, 3, 3, false); delay_ms(5000);
    CHECK(messages(F, NULL) == 0 && kept_parts() == 2, "part 2 missing: waiting");
    delay_ms(9 * 60 * 1000);
    CHECK(messages(F, NULL) == 0, "still waiting after 9 minutes");
    CHECK(WAIT_FOR(messages(F, NULL) == 1, 3 * 60 * 1000) && !strcmp(got, "【短信不完整】开头一段，……最后一段。") && kept_parts() == 0,
          "after 10 minutes what came is shown, marked incomplete (%.150s)", got);
    key(HAL_KEY_POWER); quiet();
    CHECK(AI("sms.thread", "{\"peer\":\"00000000016\"}") && g_rc.status == INK_RC_DONE, "open the incomplete message");
    quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture_incomplete, NULL); delay_ms(5);
}

static uint32_t draft_id;
static void make_draft(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = INK_SMS_TEXT_MAX - 1, .initial = text, .draft_page = "SmsCompose"};
    int session = ink_text_focus(&cfg, NULL, NULL);
    draft_id = session > 0 ? ink_text_draft_id() : 0;
    if (session > 0) ink_text_blur(session);
}
static volatile bool final_ready;
static ink_receipt_t final_rc;
static void final_result(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event; char status[24];
    if (ink_args_get_str(a, "status", status, sizeof status) < 0) return;
    memset(&final_rc, 0, sizeof final_rc);
    final_rc.status = !strcmp(status, "done") ? INK_RC_DONE : !strcmp(status, "partial") ? INK_RC_PARTIAL : INK_RC_FAILED;
    ink_args_get_str(a, "say", final_rc.say, sizeof final_rc.say);
    final_ready = true;
}
static ink_confirm_view_t card;
static bool send(const char *to, const char *t) {
    ink_jw_t w; ink_jw_init(&w, arguments, sizeof arguments);
    ink_jw_raw(&w, "{\"to\":"); ink_jw_str(&w, to);
    if (strlen(t) + strlen(to) + 10 > INK_ARGS_TEXT) {
        CHECK(t == text, "long send uses the draft fixture"); on_ui(make_draft);
        ink_jw_raw(&w, ",\"page\":\"SmsCompose\",\"draft\":"); ink_jw_int(&w, draft_id);
    } else { ink_jw_raw(&w, ",\"text\":"); ink_jw_str(&w, t); }
    ink_jw_raw(&w, "}");
    if (!BY_HAND("message.send", arguments)) return false;
    if (g_rc.status != INK_RC_PENDING_CONFIRMATION) return true;
    final_ready = false;
    if (ink_confirm_list(&card, 1) != 1 || ink_confirm_decide(card.token, true, INK_SRC_TOUCH)) return false;
    if (!WAIT_FOR(final_ready, 15000)) return false;
    g_rc = final_rc; return true;
}
static void card_snapshot(void) { if (ink_ui_snapshot(g_result, sizeof g_result, NULL) <= 0) g_result[0] = 0; }
/* The AI asks; the card a person says yes to tells how many billed parts, as the composer does. */
static bool ai_send(const char *t) {
    ink_jw_t w; ink_jw_init(&w, arguments, sizeof arguments);
    ink_jw_raw(&w, "{\"to\":\"10010\",\"text\":"); ink_jw_str(&w, t); ink_jw_raw(&w, "}");
    return AI("message.send", arguments) && g_rc.status == INK_RC_PENDING_CONFIRMATION && ink_confirm_list(&card, 1) == 1;
}
static void ai_confirm(void) {
    int before = sim_net_sms_sent();
    han(text, 100, "汉");
    CHECK(ai_send(text) && strstr(card.detail, "汉汉汉汉汉汉汉汉汉汉汉汉汉汉汉汉汉汉汉汉…") && strstr(card.detail, "共 100 字，将按 2 条发送") && strlen(card.detail) < 160,
          "the AI's 100 characters: bounded card says 2 parts (%d bytes)", (int)strlen(card.detail));
    CHECK(WAIT_FOR(({ on_ui(card_snapshot); strstr(g_result, "将按 2 条发送") != NULL; }), 5000), "and so does the card's snapshot (%d bytes)", (int)strlen(g_result));
    CHECK(sim_net_sms_sent() == before && ink_confirm_decide(card.token, true, INK_SRC_TOUCH) == 0, "nothing goes before the yes");
    CHECK(WAIT_FOR(sim_net_sms_sent() == before + 2, 10000), "after the yes, 2 parts go (%d)", sim_net_sms_sent() - before);
    quiet();
    CHECK(ai_send("短的") && !strstr(card.detail, "将按") && !strcmp(card.detail, "发短信给 10010：短的"), "one part: no count on the card (%.150s)", card.detail);
    CHECK(ink_confirm_decide(card.token, false, INK_SRC_TOUCH) == 0, "declined");
    han(text, 150, "汉");
    ink_jw_t w; ink_jw_init(&w, arguments, sizeof arguments);
    ink_jw_raw(&w, "{\"to\":\"10010\",\"text\":"); ink_jw_str(&w, text); ink_jw_raw(&w, "}");
    before = sim_net_sms_sent();
    CHECK(AI("message.send", arguments) && g_rc.status == INK_RC_INVALID && strstr(g_rc.detail, "416-byte") && ink_confirm_list(&card, 1) == 0 && sim_net_sms_sent() == before,
          "AI text beyond the table: explicit refusal, no card or send (%d, %s)", g_rc.status, g_rc.detail);
    on_ui(make_draft); snprintf(arguments, sizeof arguments, "{\"to\":\"10010\",\"page\":\"SmsCompose\",\"draft\":%u}", (unsigned)draft_id);
    CHECK(AI("message.send", arguments) && g_rc.status == INK_RC_INVALID && strstr(g_rc.say, "AI 不能") && ink_confirm_list(&card, 1) == 0 && sim_net_sms_sent() == before,
          "AI cannot bypass the text limit with a draft (%d)", g_rc.status);
}
static void transmit(void) {
    CHECK(ink_cap_subscribe("confirm.result", final_result, NULL, INK_EXEC_DIRECT) == 0, "observe final send receipts");
    char status[96], first[3] = "", ref[3] = "", pnum[32];
    han(text, 150, "汉");
    CHECK(send("10086", text) && g_rc.status == INK_RC_DONE && !strcmp(g_rc.say, "已发出"), "150 Chinese characters go (%s)", g_rc.say);
    CHECK(sim_net_sms_sent() == 3, "as 3 parts (%d)", sim_net_sms_sent());
    for (int p = 0; p < 3; p++) {
        CHECK(sim_net_sent_pdu(p, pdu, sizeof pdu) == 3, "PDU %d is there", p);
        snprintf(ref, sizeof ref, "%.2s", pdu + 28);
        if (!p) snprintf(first, sizeof first, "%s", ref);
        snprintf(pnum, sizeof pnum, "050003%s03%02d", first, p + 1);
        CHECK(!strncmp(pdu, "0041", 4) && !strncmp(pdu + 18, "08", 2) && !strncmp(pdu + 22, pnum, 12) && !strcmp(ref, first),
              "part %d: UDHI, UCS2, header 05 00 03 with one reference (%.40s)", p + 1, pdu);
    }
    CHECK(sim_net_sent_sms(0, pnum, sizeof pnum, got, sizeof got) == 1 && !strcmp(got, text) && !strcmp(pnum, "10086"), "the far phone has all of it");
    CHECK(messages("10086", status) == 1 && !strcmp(status, "已发出"), "recorded as sent");
    CHECK(send("10010", "你好") && g_rc.status == INK_RC_DONE && sim_net_sms_sent() == 4, "a short message is one PDU");
    CHECK(sim_net_sent_pdu(3, pdu, sizeof pdu) == 4 && !strncmp(pdu, "0001", 4) && sim_net_sent_sms(1, pnum, sizeof pnum, got, sizeof got) == 2 &&
          !strcmp(got, "你好"), "without a header, as written (%.40s)", pdu);
    memset(text, 'a', 200); text[200] = 0;
    CHECK(send("10010", text) && g_rc.status == INK_RC_DONE && sim_net_sms_sent() == 6, "200 GSM characters: 2 parts");
    CHECK(sim_net_sent_pdu(4, pdu, sizeof pdu) == 6 && !strncmp(pdu + 18, "00A0", 4) && sim_net_sent_sms(2, pnum, sizeof pnum, got, sizeof got) == 3 &&
          !strcmp(got, text), "GSM 7-bit, 160 septets in part 1 (%.40s)", pdu);
    han(text, 150, "汉");
    fail_nth = 2; vtk_event_at(vtk_now() + SIM_MS(1), fail_part, NULL); delay_ms(5);
    CHECK(send("10000", text) && g_rc.status == INK_RC_PARTIAL, "part 2 refused: partly sent (%d)", g_rc.status);
    CHECK(!strcmp(g_rc.say, "共 3 条：前 1 条已发出，第 2 条结果未知，请先核实，后 1 条未发出") && sim_net_sms_sent() == 7,
          "the receipt says which went (%s)", g_rc.say);
    CHECK(messages("10000", status) == 1 && !strcmp(status, g_rc.say), "and so does the record (%s)", status);
    han(text, 5 * 67 + 1, "汉");
    CHECK(send("10000", text) && g_rc.status == INK_RC_INVALID && strstr(g_rc.say, "最多分 5 条") && sim_net_sms_sent() == 7,
          "longer than 5 parts is refused, nothing goes (%s)", g_rc.say);
    han(text, 5 * 67, "汉");
    CHECK(send("10000", text) && g_rc.status == INK_RC_DONE && sim_net_sms_sent() == 12, "5 parts go");
    ai_confirm();
}

static void type_text(void) { ink_text_commit_utf8(text); }
static void finish_text(void) { ink_text_enter(); }
static void locate_send(void) {
    lv_area_t area; lv_obj_update_layout(S.send); lv_obj_get_coords(S.send, &area);
    g_btn_x[0] = (area.x1 + area.x2) / 2; g_btn_y[0] = (area.y1 + area.y2) / 2;
}
static void snap_compose(void) { if (ink_ui_snapshot(g_result, sizeof g_result, NULL) <= 0) g_result[0] = 0; }
static void compose(void) {
    CHECK(AI("contacts.put", "{\"name\":\"合成收件人\",\"number\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "a synthetic contact");
    CHECK(BY_HAND("shell.launch", "{\"id\":\"message\"}") && g_rc.status == INK_RC_DONE, "open SMS");
    CHECK(WAIT_FOR(S.list != NULL, 10000), "SMS list"); quiet();
    CHECK(BY_HAND("sms.compose", NULL) && g_rc.status == INK_RC_DONE, "compose"); CHECK(WAIT_FOR(S.recipient != NULL, 10000), "recipient input"); quiet();
    snprintf(arguments, sizeof arguments, "{\"id\":%u}", 1u);
    CHECK(AI("sms.recipient", arguments) && WAIT_FOR(S.field != NULL, 10000), "recipient chosen"); quiet();
    CHECK(AI("sms.text", NULL) && g_rc.status == INK_RC_DONE, "focus the text");
    han(text, 60, "短"); on_ui(type_text); quiet();
    CHECK(!strcmp(lv_label_get_text(S.parts), ""), "one part: nothing to say (%s)", lv_label_get_text(S.parts));
    han(text, 90, "短"); on_ui(type_text); quiet();
    CHECK(!strcmp(lv_label_get_text(S.parts), "将按 3 条发送"), "150 characters: 3 parts (%s)", lv_label_get_text(S.parts));
    on_ui(snap_compose); CHECK(strstr(g_result, "发送短信，将按 3 条发送") != NULL, "the AI's snapshot says it too (%d bytes)", (int)strlen(g_result));
    vtk_event_at(vtk_now() + SIM_MS(1), capture_parts, NULL); delay_ms(5);
    on_ui(finish_text); quiet(); on_ui(locate_send); int before = sim_net_sms_sent();
    tap(g_btn_x[0], g_btn_y[0]);
    CHECK(WAIT_FOR(ink_confirm_list(&card, 1) == 1, 10000) && strstr(card.detail, "共 150 字，将按 3 条发送") && strlen(card.detail) < 160,
          "physical send loads the full draft and asks about 3 billed parts (%.150s)", card.detail);
    CHECK(sim_net_sms_sent() == before && ink_confirm_decide(card.token, true, INK_SRC_TOUCH) == 0, "no part before the person's yes");
    CHECK(WAIT_FOR(sim_net_sms_sent() == before + 3 && messages("00000000001", NULL) == 1, 10000) && strlen(got) == 450,
          "after the yes the composer sends and stores all 150 characters"); quiet();
    CHECK(AI("sms.text", NULL) && g_rc.status == INK_RC_DONE, "focus the remaining draft");
    han(text, 190, "短"); on_ui(type_text); quiet();
    CHECK(!strcmp(lv_label_get_text(S.parts), "太长了，最多分 5 条发送"), "340 characters: too long (%s)", lv_label_get_text(S.parts));
}

static void run(void *arg) {
    (void)arg; vtk_host_call(read_phase, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    CHECK(WAIT_FOR(sim_net_modem_registered(), 10000), "the modem registers"); quiet();
    const char *G = "00000000017";
    if (!strcmp(phase, "rx")) receive();
    else if (!strcmp(phase, "restart-1")) {
        part(G, "断电前一，", 11, 3, 1, false); part(G, "断电前二，", 11, 3, 2, false);
        CHECK(WAIT_FOR(kept_parts() == 2 && parts_durable(), 5000) && messages(G, NULL) == 0, "two parts kept on the flash, the process ends");
    } else if (!strcmp(phase, "restart-2")) {
        CHECK(kept_parts() == 2 && messages(G, NULL) == 0, "after the restart the two parts are still there");
        part(G, "断电后三。", 11, 3, 3, false);
        CHECK(WAIT_FOR(messages(G, NULL) == 1, 5000) && !strcmp(got, "断电前一，断电前二，断电后三。") && kept_parts() == 0, "the last part completes it (%.150s)", got);
    } else if (!strcmp(phase, "tx")) transmit();
    else if (!strcmp(phase, "compose")) compose();
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "everything is written");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); osal_task_cfg_t cfg = {.name = "sms-multipart-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the long SMS test");
}
