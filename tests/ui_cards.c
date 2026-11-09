/* F5 exercises the real Today page, app.cards and the shared action bus. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static unsigned revision = 1;
static unsigned renders;
static int many_cards;
static bool overflow_cards;
static int opened;
static ink_source_t opened_by;
static char cards[2048], view[3072], context[3072], open_cap[48], open_args[64];
static uint32_t page_version;
static uint32_t alarm_id;
static int card_x, card_y;
static void empty_page(lv_obj_t *screen, void *arg) { (void)screen; (void)arg; }
static const ink_activity_t OPENED = {.name = "test.cards-open", .app = "listen", .create = empty_page};
static void open_card(ink_call_t *call) {
    opened++;
    opened_by = call->src;
    ink_ui_push(&OPENED, NULL);
    snprintf(call->rc.object, sizeof call->rc.object, "example:listen");
}
static const ink_cap_t CAPS[] = {
    {.id = "test.cards_open", .title = "打开合成听书", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = open_card}
};
static int listen_card(int index, ink_card_t *out) {
    if (index >= (many_cards ? many_cards : 2)) return 0;
    snprintf(out->member, sizeof out->member, "%s", !many_cards && index ? "kid" : "owner");
    snprintf(out->id, sizeof out->id, "example-listen");
    snprintf(out->title, sizeof out->title, "合成听书");
    snprintf(out->text, sizeof out->text, "%s合成听书%u", index ? "孩子" : "成人", revision);
    snprintf(out->action, sizeof out->action, "test.cards_open");
    out->rev = revision;
    out->has_value = true;
    out->value = revision;
    if (many_cards) {
        snprintf(out->id, sizeof out->id, "example-%d", index);
        snprintf(out->text, sizeof out->text, "成人合成听书%u \"} , ] \\\\", revision);
    }
    if (overflow_cards) { memset(out->text, '\\', sizeof out->text - 1); out->text[sizeof out->text - 1] = 0; }
    return 1;
}
static int news_card(int index, ink_card_t *out) {
    if (index || many_cards) return 0;
    snprintf(out->id, sizeof out->id, "example-news");
    snprintf(out->title, sizeof out->title, "合成报纸");
    snprintf(out->text, sizeof out->text, "成人合成报纸");
    return 1;
}
static const ink_app_def_t LISTEN = {.id = "listen", .title = "听", .entry = &OPENED, .child_ok = true, .card = listen_card};
static const ink_app_def_t NEWS = {.id = "news", .title = "报纸", .card = news_card};
static lv_obj_t *find_label(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = find_label(lv_obj_get_child(root, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}
static void cards_done(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    CHECK(rc->status == INK_RC_DONE && rc->result, "app.cards returns data");
    snprintf(cards, sizeof cards, "%s", rc->result ? rc->result : "");
}
static bool text_is(const ink_json_t *j, int object, const char *key, const char *value) {
    char text[128];
    return ink_json_str(j, ink_json_find(j, object, key), text, sizeof text) >= 0 && !strcmp(text, value);
}
static void inspect(void) {
    ink_call_opts_t opts = {.src = INK_SRC_AI, .reply_exec = INK_EXEC_DIRECT, .done = cards_done};
    CHECK(ink_cap_call("app.cards", NULL, &opts) == 0, "AI reads the production card capability");
    CHECK(!strcmp(ink_shell_board(), "Today") && ink_ui_snapshot(view, sizeof view, NULL) > 0,
          "the real Today page exports its model");
    CHECK(ink_ui_context_snapshot(context, sizeof context, NULL) > 0 && !strcmp(view, context), "AI sees exactly the Today snapshot");
    const char *mine = !strcmp(ink_member_current()->id, "kid") ? "孩子" : "成人";
    const char *other = !strcmp(mine, "孩子") ? "成人合成听书" : "孩子合成听书";
    CHECK(!strstr(cards, other) && !strstr(view, other), "other members' cards are absent from both paths");
    if (!strcmp(mine, "孩子")) CHECK(!strstr(cards, "成人合成报纸") && !strstr(view, "成人合成报纸"), "adult-only apps stay out of the child's cards");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    ink_json_t *p = osal_malloc(sizeof *p, OSAL_MEM_EXTERNAL);
    CHECK(j && p && ink_json_parse(j, cards, strlen(cards)) == 0 && ink_json_parse(p, view, strlen(view)) == 0, "both complete results are JSON");
    if (!j || !p) { osal_free(j); osal_free(p); return; }
    int card = -1, field = -1, action = -1;
    for (int t = 1; t < j->ntok; t++) if (j->tok[t].type == 1 && text_is(j, t, "app", "listen")) { card = t; break; }
    for (int t = 1; t < p->ntok; t++) if (p->tok[t].type == 1 && text_is(p, t, "id", "listen")) {
        if (ink_json_find(p, t, "role") >= 0) field = t;
        if (ink_json_find(p, t, "capability") >= 0) action = t;
    }
    char text[96], title[32], capability[48];
    ink_json_str(j, ink_json_find(j, card, "text"), text, sizeof text);
    ink_json_str(j, ink_json_find(j, card, "title"), title, sizeof title);
    ink_json_str(j, ink_json_find(j, card, "action"), capability, sizeof capability);
    int64_t data_rev = -1, view_rev = -2, version = 0;
    ink_json_int(j, ink_json_find(j, card, "rev"), &data_rev);
    ink_json_int(p, ink_json_find(p, field, "object.revision"), &view_rev);
    CHECK(card >= 0 && field >= 0 && action >= 0 && data_rev == revision && data_rev == view_rev &&
          text_is(p, field, "value", text) && text_is(p, field, "label", title) &&
          text_is(p, action, "capability", capability), "Today and app.cards share the same text, title, revision and opening capability");
    lv_obj_t *label = find_label(lv_screen_active(), text);
    CHECK(label && find_label(lv_screen_active(), title), "real LVGL labels contain those same card data");
    if (label) {
        lv_obj_t *button = lv_obj_get_parent(label);
        lv_obj_update_layout(button);
        lv_area_t area; lv_obj_get_coords(button, &area);
        card_x = (area.x1 + area.x2) / 2; card_y = (area.y1 + area.y2) / 2;
    }
    ink_json_str(p, ink_json_find(p, action, "capability"), open_cap, sizeof open_cap);
    ink_json_str(p, ink_json_find(p, action, "args_json"), open_args, sizeof open_args);
    ink_json_int(p, ink_json_find(p, 0, "page_version"), &version);
    page_version = (uint32_t)version;
    char small[16] = "previous";
    CHECK(ink_shell_home_snapshot(small, sizeof small) < 0 && !small[0], "small snapshots fail explicitly without partial data");
    osal_free(j); osal_free(p);
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the panel settles"); }
static void go_home(void) { ink_ui_home(); }
static void change(void) { revision++; ink_app_card_changed("listen"); }
static void fresh_today(void) { ink_shell_home_key(INK_KEY_NEXT, false); ink_shell_home_key(INK_KEY_PREV, false); }
static void rendered(lv_event_t *e) { (void)e; __atomic_add_fetch(&renders, 1, __ATOMIC_RELAXED); }
static unsigned render_count(void) { return __atomic_load_n(&renders, __ATOMIC_RELAXED); }
static void watch_renders(void) { lv_display_add_event_cb(lv_display_get_default(), rendered, LV_EVENT_REFR_READY, NULL); }
static void noop(void) {}
static void ai_open(void) {
    ink_receipt_t rc;
    CHECK(ink_cap_invoke_json(open_cap, open_args, INK_SRC_AI, page_version, &rc) == INK_RC_DONE &&
          !strcmp(rc.object, "example:listen"), "AI invokes the opening capability declared by Today");
}
static void switch_child(void) { CHECK(ink_member_switch("kid") == 0, "switch to the other member"); }
static void add_alarm(void) {
    ink_alarm_t alarm = {.kind = INK_ALARM, .at_utc = ink_time_now_s() + 3600, .hour = 7, .min = 45, .src = INK_SRC_TOUCH};
    snprintf(alarm.title, sizeof alarm.title, "合成事项");
    int id = ink_alarm_add(&alarm);
    CHECK(id > 0, "add a synthetic future alarm through the existing time service");
    if (id > 0) alarm_id = (uint32_t)id;
}
static void inspect_alarm(void) {
    CHECK(strstr(cards, "\"id\":\"upcoming\"") && strstr(cards, "合成事项 7:45") &&
          strstr(view, "合成事项 7:45") && find_label(lv_screen_active(), "合成事项 7:45"),
          "the existing upcoming information now shares app.cards, Today labels and the AI snapshot");
}
static void cancel_alarm(void) { CHECK(ink_alarm_cancel(alarm_id) == 0, "cancel the synthetic alarm"); }
static void inspect_no_alarm(void) {
    CHECK(!strstr(cards, "合成事项") && !strstr(view, "合成事项") && find_label(lv_screen_active(), "还没有接下来的事"),
          "cancelling updates the same source and restores the real empty state");
}
static void seven_cards(void) { many_cards = 7; change(); }
static void eight_cards(void) { many_cards = 8; change(); }
static void oversized_cards(void) { overflow_cards = true; change(); }
static void restore_cards(void) { overflow_cards = false; many_cards = 0; change(); }
static void inspect_many(void) {
    ink_call_opts_t opts = {.src = INK_SRC_AI, .reply_exec = INK_EXEC_DIRECT, .done = cards_done};
    CHECK(ink_cap_call("app.cards", NULL, &opts) == 0, "AI reads the complete multi-card receipt");
    int count = 0;
    for (const char *p = cards; (p = strstr(p, "\"app\":\"listen\"")); p++) count++;
    CHECK(count == many_cards && strlen(cards) < sizeof cards - 1, "all seven or eight cards fit the unchanged receipt budget");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && (ink_json_parse(j, cards, strlen(cards)) == 0) == (many_cards == 7),
          "the eighth card crosses the unchanged whole-array token limit");
    CHECK(ink_ui_snapshot(view, sizeof view, NULL) > 0 && ink_ui_context_snapshot(context, sizeof context, NULL) > 0 &&
          !strcmp(view, context), "multi-card Today and AI page snapshots remain identical");
    ink_card_t expected = {0}; listen_card(0, &expected);
    bool first = false, action = false;
    if (j && ink_json_parse(j, view, strlen(view)) == 0)
        for (int t = 1; t < j->ntok; t++) if (j->tok[t].type == 1 && text_is(j, t, "id", "listen")) {
            if (text_is(j, t, "value", expected.text) && text_is(j, t, "label", expected.title)) first = true;
            if (text_is(j, t, "capability", expected.action)) action = true;
        }
    CHECK(first && action && find_label(lv_screen_active(), expected.text),
          "Today retains the first card and its action, including escaped quotes, braces and backslashes");
    inspect_alarm();
    osal_free(j);
}
static void cards_failed(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    CHECK(rc->status == INK_RC_FAILED && !rc->result, "an oversized source fails explicitly");
}
static void inspect_failed(void) {
    ink_call_opts_t opts = {.src = INK_SRC_AI, .reply_exec = INK_EXEC_DIRECT, .done = cards_failed};
    CHECK(ink_cap_call("app.cards", NULL, &opts) == 0, "AI receives the source failure");
    snprintf(view, sizeof view, "previous");
    CHECK(ink_ui_snapshot(view, sizeof view, NULL) < 0 && !view[0], "a failed card read exports no partial snapshot");
    CHECK(find_label(lv_screen_active(), "卡片没能读取") && !find_label(lv_screen_active(), "还没有在听的内容"),
          "failed card reads show an error instead of a false empty state");
}
static void take_snapshot(void *name) { sim_display_snapshot(name); }
static void capture(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), take_snapshot, (void *)name); delay_ms(5); }
static void run(void *arg) {
    (void)arg;
    ink_power_hold(INK_NEED_NET); /* Keep idle status-bar redraws outside the card render measurements. */
    delay_ms(1800); quiet();
    on_ui(watch_renders);
    unsigned drawn = render_count(), committed = ink_display_commits();
    double before = sim_metric_value("home.cards_updates");
    for (int i = 0; i < 4; i++) { on_ui(change); delay_ms(40); }
    delay_ms(1100);
    CHECK(sim_metric_value("home.cards_updates") == before, "hidden Today performs zero updates on Main");
    CHECK(render_count() == drawn && ink_display_commits() == committed, "hidden card changes perform zero renders and display commits");
    on_ui(add_alarm);
    CHECK(BY_HAND("home.prev", NULL) && g_rc.status == INK_RC_DONE, "open Today"); quiet(); on_ui(inspect);
    on_ui(inspect_alarm);
    capture("today-adult");
    drawn = render_count(); committed = ink_display_commits();
    on_ui(fresh_today);
    for (int i = 0; i < 10; i++) { on_ui(noop); delay_ms(20); }
    unsigned page_renders = render_count() - drawn, page_commits = ink_display_commits() - committed;
    quiet();
    CHECK(page_renders == 1 && page_commits == 1 && render_count() == drawn + 1 && ink_display_commits() == committed + 1,
          "the no-change control renders and commits only the page turn");
    drawn = render_count(); committed = ink_display_commits();
    on_ui(fresh_today);
    before = sim_metric_value("home.cards_updates");
    for (int i = 0; i < 10; i++) { on_ui(change); delay_ms(20); }
    CHECK(sim_metric_value("home.cards_updates") == before, "ten changes within the throttle interval do not redraw early");
    CHECK(render_count() - drawn == page_renders && ink_display_commits() - committed == page_commits,
          "the visible burst performs zero early renders and commits beyond the page-turn control");
    CHECK(WAIT_FOR(sim_metric_value("home.cards_updates") > before, 1500), "the trailing update arrives without another change");
    CHECK(sim_metric_value("home.cards_updates") == before + 1, "ten visible changes coalesce to exactly one update");
    sim_metric_set("test.cards_burst_updates", sim_metric_value("home.cards_updates") - before);
    on_ui(noop); quiet();
    CHECK(render_count() - drawn == page_renders + 1 && ink_display_commits() - committed == page_commits + 1,
          "the visible burst produces exactly one render and display commit beyond the page-turn control");
    sim_metric_set("test.cards_burst_renders", render_count() - drawn - page_renders);
    sim_metric_set("test.cards_burst_commits", ink_display_commits() - committed - page_commits);
    on_ui(inspect);
    before = sim_metric_value("home.cards_updates"); delay_ms(1200);
    CHECK(sim_metric_value("home.cards_updates") == before, "unchanged visible cards have no heartbeat updates");
    on_ui(cancel_alarm);
    CHECK(WAIT_FOR(sim_metric_value("home.cards_updates") > before, 1500), "alarm changes also update Today through app.cards_rev");
    quiet(); on_ui(inspect); on_ui(inspect_no_alarm);
    CHECK(BY_HAND("shell.control", NULL), "cover Today with a real overlay"); quiet();
    drawn = render_count(); committed = ink_display_commits();
    before = sim_metric_value("home.cards_updates");
    for (int i = 0; i < 4; i++) { on_ui(change); delay_ms(40); }
    delay_ms(1100); quiet();
    CHECK(sim_metric_value("home.cards_updates") == before, "covered Today performs zero updates");
    CHECK(render_count() == drawn && ink_display_commits() == committed, "covered card changes perform zero renders and display commits");
    sim_metric_set("test.cards_covered_commits", ink_display_commits() - committed);
    CHECK(BY_HAND("shell.close", NULL), "uncover Today"); quiet();
    CHECK(sim_metric_value("home.cards_updates") == before + 1, "uncovering reads the pending change once"); on_ui(inspect);
    int was_opened = opened;
    tap(card_x, card_y); quiet();
    CHECK(opened == was_opened + 1 && opened_by == INK_SRC_TOUCH && !strcmp(ink_ui_page_name(), OPENED.name), "a real card tap opens through the declared capability");
    before = sim_metric_value("home.cards_updates"); on_ui(change); delay_ms(1100);
    CHECK(sim_metric_value("home.cards_updates") == before, "Today performs zero updates under another activity");
    on_ui(go_home); quiet(); on_ui(inspect); on_ui(ai_open); quiet();
    CHECK(opened == was_opened + 2 && opened_by == INK_SRC_AI && !strcmp(ink_ui_page_name(), OPENED.name), "AI and touch open the same real page through the same handler");
    on_ui(go_home); quiet(); on_ui(switch_child); quiet();
    CHECK(BY_HAND("home.prev", NULL), "the child opens their Today"); quiet(); on_ui(inspect); capture("today-child");
    ink_member_test_reset(); quiet();
    CHECK(BY_HAND("home.prev", NULL), "the adult returns to their Today"); quiet(); on_ui(inspect);
    on_ui(add_alarm);
    before = sim_metric_value("home.cards_updates"); on_ui(seven_cards);
    CHECK(WAIT_FOR(sim_metric_value("home.cards_updates") > before, 1500), "seven cards update Today"); quiet(); on_ui(inspect_many);
    before = sim_metric_value("home.cards_updates"); on_ui(eight_cards);
    CHECK(WAIT_FOR(sim_metric_value("home.cards_updates") > before, 1500), "eight cards update Today"); quiet(); on_ui(inspect_many); capture("today-eight-cards");
    before = sim_metric_value("home.cards_updates"); on_ui(oversized_cards);
    CHECK(WAIT_FOR(sim_metric_value("home.cards_updates") > before, 1500), "a source error reaches Today"); quiet(); on_ui(inspect_failed);
    before = sim_metric_value("home.cards_updates"); on_ui(restore_cards);
    CHECK(WAIT_FOR(sim_metric_value("home.cards_updates") > before, 1500), "cards recover on the next ordinary change"); quiet(); on_ui(inspect); on_ui(inspect_alarm);
    ink_power_release(INK_NEED_NET);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    CHECK(ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) == 0 && ink_app_register(&LISTEN) == 0 && ink_app_register(&NEWS) == 0,
          "register only synthetic test providers");
    osal_task_cfg_t cfg = {.name = "ui-cards-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the bounded test driver");
}
