/* Synthetic whole-product notification management and restart checks. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

static char snapshot[4096], scene[64], phase[16];
static const char *wanted, *wanted_cap;
static lv_obj_t *label, *target;
static int x, y;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static void read_page(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "page exports AI snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "page fits production AI context");
}
static bool page_has(const char *s) { on_ui(read_page); return strstr(snapshot, s) != NULL; }
static void find_label(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) label = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, (int32_t)i));
}
static void find_cap(lv_obj_t *o) {
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strcmp(cap, wanted_cap)) target = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_cap(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) {
    label = target = NULL; x = y = -1;
    lv_obj_update_layout(lv_screen_active()); find_label(lv_screen_active());
    if (!label) return;
    find_cap(lv_obj_get_parent(label));
    if (!target) return;
    lv_area_t a; lv_obj_get_coords(target, &a);
    CHECK(a.x1 >= 0 && a.x2 < ink_ui_px(INK_CANVAS_W) && a.y1 >= 0 && a.y2 < ink_ui_px(INK_CANVAS_H), "touch target fits screen");
    CHECK(a.x2 - a.x1 + 1 >= ink_ui_px(INK_CONTROL) && a.y2 - a.y1 + 1 >= ink_ui_px(INK_CONTROL), "touch target meets minimum size");
    x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
}
static bool locate_label(const char *text, const char *cap) {
    wanted = text; wanted_cap = cap; on_ui(locate); return x >= 0;
}
static void press(const char *text, const char *cap) {
    quiet(); CHECK(locate_label(text, cap), "visible label binds %s: %s", cap, text);
    if (x >= 0) tap(x, y);
    quiet();
}
static void save_snapshot(void *arg) {
    (void)arg; char path[768];
    int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, scene);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void capture(void *arg) { sim_display_snapshot(arg); }
static void evidence(const char *name) {
    snprintf(scene, sizeof scene, "%s", name); quiet(); on_ui(read_page);
    vtk_host_call(save_snapshot, NULL); vtk_event_at(vtk_now() + SIM_MS(1), capture, scene); delay_ms(5);
}
static void setting(const char *id, int v) {
    char args[96]; snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%d}", id, v);
    CHECK(AI("settings.set", args) && g_rc.status == INK_RC_DONE, "change %s", id); quiet();
}
static int count(void) {
    ink_db_cursor_t c = {0}; ink_notify_view_t n; int total = 0;
    while (ink_notify_next(&c, &n) == 1) total++;
    return total;
}
static uint32_t find_notice(const char *title) {
    ink_db_cursor_t c = {0}; ink_notify_view_t n;
    while (ink_notify_next(&c, &n) == 1) if (!strcmp(n.title, title)) return n.id;
    return 0;
}
static uint32_t post(const char *title) {
    uint32_t id = 0;
    ink_notify_t n = {.channel = "ai", .title = title, .body = "SYNTH body", .prio = INK_NOTIFY_LOW};
    CHECK(ink_notify_post(&n, &id) == 0 && id, "seed synthetic notice: %s", title); return id;
}
static void dismiss_ai(uint32_t id) {
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id);
    CHECK(AI("notify.dismiss", args) && g_rc.status == INK_RC_DONE, "AI dismisses through the same capability"); quiet();
}
static void deletion(void) {
    CHECK(ink_notify_clear() == 0, "start with clean synthetic inbox");
    uint32_t keep_id = post("SYNTH_KEEP"), touch_id = post("SYNTH_TOUCH"), ai_id = post("SYNTH_AI");
    CHECK(count() == 3 && ink_notify_unread() == 3, "three independent unread notices");
    CHECK(AI("shell.notifications", NULL) && g_rc.status == INK_RC_DONE, "open notification list"); quiet();
    CHECK(page_has("notify.dismiss") && page_has("SYNTH_TOUCH"), "snapshot exposes per-notification dismiss arguments");
    evidence("notifications-zh"); setting("system.language", 1); evidence("notifications-en");
    CHECK(locate_label("Delete", "notify.dismiss"), "English delete button translated"); setting("system.language", 0);
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)touch_id);
    CHECK(AI("notify.read", args) && g_rc.status == INK_RC_DONE, "make the touch target already read"); quiet();
    press("SYNTH_TOUCH", "notify.dismiss");
    CHECK(WAIT_FOR(!find_notice("SYNTH_TOUCH"), 3000), "touch deletes only the chosen notice");
    CHECK(count() == 2 && ink_notify_unread() == 2 && find_notice("SYNTH_KEEP") && find_notice("SYNTH_AI"), "other notices and unread count stay correct");
    CHECK(!page_has("SYNTH_TOUCH") && page_has("\"total\":2"), "read-notice deletion refreshes list without unread change");
    dismiss_ai(ai_id);
    CHECK(count() == 1 && ink_notify_unread() == 1 && !page_has("SYNTH_AI"), "AI deletion updates list and unread count");
    post("SYNTH_UNREAD"); quiet(); press("SYNTH_UNREAD", "notify.dismiss");
    CHECK(count() == 1 && ink_notify_unread() == 1 && find_notice("SYNTH_KEEP"), "touch deletion of unread notice decrements count");
    uint32_t extra[5];
    const char *body = "SYNTH notification body that is deliberately long enough to exercise the production AI context together with every visible action and notification title.";
    for (unsigned i = 0; i < sizeof extra / sizeof *extra; i++) {
        char title[48]; snprintf(title, sizeof title, "SYNTH_LONG_NOTIFICATION_WITH_BODY_%u", i);
        ink_notify_t n = {.channel = "ai", .title = title, .body = body, .prio = INK_NOTIFY_LOW};
        CHECK(ink_notify_post(&n, &extra[i]) == 0, "post full-page notification fixture");
    }
    quiet(); on_ui(read_page);
    CHECK(AI("shell.notify_next", NULL), "visit notification second page"); quiet();
    CHECK(page_has("SYNTH_KEEP") && !page_has("SYNTH_LONG_NOTIFICATION_WITH_BODY_4"), "snapshot actions belong only to the current page");
    CHECK(AI("shell.notify_prev", NULL), "return to notification first page"); quiet();
    char keep_object[64]; snprintf(keep_object, sizeof keep_object, "\"object\":\"notify:%u\"", (unsigned)keep_id);
    CHECK(!page_has("SYNTH_KEEP") && !page_has(keep_object), "returned notification page has no stale fields or actions");
    setting("display.font_size", 3); evidence("notifications-zh-large");
    setting("system.language", 1); evidence("notifications-en-large");
    setting("system.language", 0); setting("display.font_size", 1);
    for (unsigned i = 0; i < sizeof extra / sizeof *extra; i++) dismiss_ai(extra[i]);
}
static void sms(void) {
    snprintf(g_hw.num, sizeof g_hw.num, "10000000000"); snprintf(g_hw.text, sizeof g_hw.text, "SYNTH_SMS");
    hw(HW_SMS, 0, 0);
}
static void channels(void) {
    CHECK(AI("shell.settings", NULL) && g_rc.status == INK_RC_DONE, "open settings directory"); quiet();
    press("声音", "shell.sound");
    for (int i = 0; i < 3 && !locate_label("通知", "shell.notify_settings"); i++) {
        CHECK(AI("shell.page_next", NULL), "visit next sound settings page"); quiet();
    }
    CHECK(locate_label("通知", "shell.notify_settings"), "sound settings has a notification entry"); press("通知", "shell.notify_settings");
    CHECK(!strcmp(ink_ui_page_name(), "SetNotifications"), "touch opens notification settings");
    const ink_notify_channel_t *ch;
    for (int i = 0; (ch = ink_notify_channel_at(i)); i++) {
        if (!page_has(ch->id)) { CHECK(AI("shell.page_next", NULL), "visit next channel page"); quiet(); }
        CHECK(page_has(ch->id) && page_has("notify.set_channel"), "registered channel is visible to AI: %s", ch->id);
    }
    CHECK(AI("shell.notify_settings", NULL), "return to first channel page"); quiet(); evidence("channels-zh");
    setting("system.language", 1); CHECK(page_has("Messages"), "channel labels translated"); evidence("channels-en");
    setting("display.font_size", 3); evidence("channels-en-large");
    setting("system.language", 0); evidence("channels-zh-large"); setting("display.font_size", 1);
    press("短信", "notify.set_channel");
    CHECK(WAIT_FOR(!ink_notify_channel_on("message"), 3000), "touch disables message channel");
    CHECK(WAIT_FOR(page_has("\"enabled\":false"), 3000), "channel snapshot refreshes after touch");
    int before = count(); sms();
    CHECK(WAIT_FOR(sim_metric_value("notify.channel_off") >= 1, 5000), "new SMS reaches disabled notification channel");
    CHECK(count() == before && ink_notify_unread() == before, "disabled SMS adds no notification or unread count");
    CHECK(AI("message.list", NULL) && strstr(g_rc.say, "SYNTH_SMS"), "disabled notifications preserve received SMS");
    press("短信", "notify.set_channel");
    CHECK(WAIT_FOR(ink_notify_channel_on("message"), 3000), "touch enables message channel again");
    sms(); CHECK(WAIT_FOR(count() == before + 1, 5000), "new SMS notification resumes after enabling");
    CHECK(AI("notify.set_channel", "{\"id\":\"message\",\"enabled\":false}") && g_rc.status == INK_RC_DONE, "AI changes channel through same capability");
    CHECK(WAIT_FOR(page_has("\"enabled\":false"), 3000), "AI channel change refreshes current page");
    CHECK(ink_notify_channel_on("system") && ink_notify_channel_on("ai"), "other channel preferences unaffected");
    uint32_t sms_id = find_notice("10000000000"); if (sms_id) dismiss_ai(sms_id);
}
static void child_rules(void) {
    CHECK(ink_member_switch("kid") == 0 && WAIT_FOR(!strcmp(ink_member_current()->id, "kid"), 3000), "switch to child"); quiet();
    CHECK(WAIT_FOR(ink_notify_channel_on("message"), 5000), "child has independent channel preferences");
    CHECK(AI("shell.notify_settings", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot open notification settings by AI");
    CHECK(BY_HAND("shell.notify_settings", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot open notification settings by touch capability");
    CHECK(AI("notify.set_channel", "{\"id\":\"message\",\"enabled\":false}") && g_rc.status == INK_RC_REJECTED, "child channel edits rejected for AI");
    CHECK(BY_HAND("notify.set_channel", "{\"id\":\"message\",\"enabled\":false}") && g_rc.status == INK_RC_REJECTED, "child channel edits rejected for touch");
    CHECK(!ink_cap_ai_callable(ink_cap_find("notify.set_channel")) && ink_notify_channel_on("message"), "child AI cannot discover or change channel setting");
    CHECK(AI("shell.control", NULL), "open child control centre"); quiet();
    CHECK(!locate_label("通知", "shell.notify_settings"), "child has no channel settings control"); CHECK(AI("shell.close", NULL), "close child controls");
    uint32_t kid_id = post("SYNTH_KID");
    CHECK(AI("shell.notifications", NULL), "child may read own notifications"); quiet();
    CHECK(!page_has("SYNTH_KEEP"), "adult inbox not exposed to child");
    press("SYNTH_KID", "notify.dismiss"); CHECK(!find_notice("SYNTH_KID"), "child may dismiss own notification by touch");
    kid_id = post("SYNTH_KID_AI"); dismiss_ai(kid_id); CHECK(count() == 0, "child may dismiss own notification by AI");
    /* the page redraws from the notify.unread change, after the AI's dismissal returns */
    CHECK(WAIT_FOR(page_has("\"total\":0") && !page_has("notify.dismiss"), 3000), "last deletion removes all snapshot dismiss actions");
    ink_member_test_reset();
    CHECK(WAIT_FOR(!strcmp(ink_member_current()->id, "owner") && !ink_notify_channel_on("message"), 5000), "adult preferences stay isolated");
    CHECK(count() == 1 && find_notice("SYNTH_KEEP"), "adult notification survives child operations");
}
static void get_phase(void *arg) {
    (void)arg; const char *v = getenv("INK_NOTIFY_PHASE"); snprintf(phase, sizeof phase, "%s", v ? v : "main");
}
static void run(void *arg) {
    (void)arg; vtk_host_call(get_phase, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    if (!strcmp(phase, "restart")) {
        CHECK(count() == 1 && find_notice("SYNTH_KEEP") && ink_notify_unread() == 1, "restart preserves only the untouched notification");
        CHECK(!find_notice("SYNTH_TOUCH") && !find_notice("SYNTH_AI") && !find_notice("SYNTH_UNREAD"), "dismissed notifications do not return after restart");
        CHECK(!ink_notify_channel_on("message") && ink_notify_channel_on("system"), "channel choice persists after restart");
        CHECK(AI("notify.set_channel", "{\"id\":\"message\",\"enabled\":true}") && g_rc.status == INK_RC_DONE, "AI enables channel after restart");
        sms(); CHECK(WAIT_FOR(count() == 2, 5000), "enabled SMS channel works after restart");
    } else { deletion(); channels(); child_rules(); }
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 15000), "all writes durable before restart");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "notify-manage", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("notification management test");
}
