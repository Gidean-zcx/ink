/* lock-1: the lock cover over the page (D-135). What it shows and hides, what it redraws, its touch and AI paths.
 * INK_LOCK_COVER_MODE: "" every check; "baseline" the same scenes on an older lock source (pictures only, built with
 * INK_LOCK_COVER_BASELINE); "reverse" the privacy part only, on a copy whose cover is see-through: it must fail. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#include "ink/sys/notify.h"
#include "ink/sys/time.h"
#include "src/display/lv_display_private.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include "../system/ui/shell/member_pages.h"
#ifndef INK_TEST_LOCK_SOURCE
#define INK_TEST_LOCK_SOURCE "../system/ui/lock_mark.c"
#endif
#include INK_TEST_LOCK_SOURCE

static char mode[16], frame_name[48], snapshot[4096], wanted[64];
static int size = 1;
static bool captured, found;
static lv_obj_t *hit;
static lv_area_t box;
static void host_env(void *arg) {
    (void)arg;
    const char *m = getenv("INK_LOCK_COVER_MODE"), *s = getenv("INK_LOCK_COVER_SIZE");
    snprintf(mode, sizeof mode, "%.15s", m ? m : "");
    size = s ? atoi(s) : 1;
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "display settles"); }
static void capture(void *arg) { sim_display_snapshot(arg); captured = true; }
static void frame(const char *name) {
    quiet(); snprintf(frame_name, sizeof frame_name, "%s", name); captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), capture, frame_name);
    CHECK(WAIT_FOR(captured, 1000), "capture %s", name);
}
static void setting(const char *id, int value) {
    ink_receipt_t rc;
    CHECK(ink_setting_set(id, value, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "setting %s passes its policy", id); quiet();
}
static void lock(void) {
    ink_tm_t t = {.year = 2026, .month = 11, .day = 12, .hour = 14, .min = 5};
    ink_time_set(ink_time_from_local(&t), false); quiet(); /* every picture at Thursday 14:05 */
    CHECK(BY_HAND("shell.lock", NULL) && WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 3000), "the device locks"); quiet();
}
static void wake(void) { key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() != INK_PWR_LOCKED, 3000), "the power key wakes it"); quiet(); }
static void read_snapshot(void) { if (ink_ui_context_snapshot(snapshot, sizeof snapshot, NULL) < 0) snapshot[0] = 0; }
static bool shows(const char *text) { on_ui(read_snapshot); return strstr(snapshot, text) != NULL; }
static void sms(const char *body) {
    ink_notify_t n = {.channel = "message", .title = "合成发信人", .body = body, .group = "合成发信人", .prio = -1, .src = INK_SRC_SYSTEM};
    CHECK(ink_notify_post(&n, NULL) == 0, "a synthetic message arrives");
}
static void seed(void) {
    sms("合成短信正文甲"); sms("合成短信正文乙");
    ink_notify_t call = {.channel = "call", .title = "未接来电", .body = "00000000002", .group = "missed-calls", .prio = INK_NOTIFY_NORMAL, .src = INK_SRC_SYSTEM};
    CHECK(ink_notify_post(&call, NULL) == 0, "a synthetic missed call arrives");
    CHECK(BY_HAND("reminder.add", "{\"at\":\"2026-11-12 15:00\",\"title\":\"合成接孩子\",\"ring\":false}") && g_rc.status == INK_RC_DONE,
          "an event later today: %s %.80s", ink_rc_name(g_rc.status), g_rc.detail);
    CHECK(WAIT_FOR(ink_notify_unread() == 3, 5000), "three unread notifications");
}
/* Labels whose text is wanted (exactly, or contained when part), last one first: what is drawn on top. */
static bool part, button;
static void search(lv_obj_t *root) {
    for (int32_t i = (int32_t)lv_obj_get_child_count(root) - 1; !found && i >= 0; i--) {
        lv_obj_t *o = lv_obj_get_child(root, i);
        search(o);
        if (found || !lv_obj_check_type(o, &lv_label_class)) continue;
        const char *text = lv_label_get_text(o);
        if ((part ? strstr(text, wanted) == NULL : strcmp(text, wanted) != 0) || (button && !lv_obj_check_type(root, &lv_button_class))) continue;
        hit = lv_obj_check_type(root, &lv_button_class) ? root : o; found = true;
        lv_obj_update_layout(hit); lv_obj_get_coords(hit, &box);
    }
}
static void look(void) { found = false; hit = NULL; search(lv_layer_sys()); }
static bool visible(const char *text) { part = false; snprintf(wanted, sizeof wanted, "%s", text); on_ui(look); return found; }
static bool drawn(const char *text) { part = true; snprintf(wanted, sizeof wanted, "%s", text); on_ui(look); part = false; return found; }
static bool picture(const char *initial) { button = true; bool ok = visible(initial); button = false; return ok; }
static void press_picture(const char *initial) {
    CHECK(picture(initial), "the picture %s is on the screen", initial);
    if (found) tap((box.x1 + box.x2) / 2, (box.y1 + box.y2) / 2);
    quiet();
}
static void open_pin(void) { ink_shell_member_pin_open("owner"); }
/* The PIN page is the newest surface on the system layer: its own emergency call and camera. */
static bool pin_emergency, pin_camera;
static void bound(lv_obj_t *root) {
    const char *cap = ink_shell_bound_cap(root);
    if (cap && !strcmp(cap, "app.open")) pin_camera = true;
    if (cap && !strcmp(cap, "phone.emergency_open")) pin_emergency = true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) bound(lv_obj_get_child(root, (int32_t)i));
}
static void pin_entries(void) {
    pin_emergency = pin_camera = false;
    uint32_t n = lv_obj_get_child_count(lv_layer_sys());
    if (n) bound(lv_obj_get_child(lv_layer_sys(), (int32_t)n - 1));
}

#ifndef INK_LOCK_COVER_BASELINE
static void probe_area(const char *name, lv_obj_t *o) {
    lv_area_t a; lv_obj_update_layout(o); lv_obj_get_coords(o, &a);
    char key[48];
    snprintf(key, sizeof key, "test.%s.x1", name); ink_probe_value(key, a.x1);
    snprintf(key, sizeof key, "test.%s.y1", name); ink_probe_value(key, a.y1);
    snprintf(key, sizeof key, "test.%s.x2", name); ink_probe_value(key, a.x2);
    snprintf(key, sizeof key, "test.%s.y2", name); ink_probe_value(key, a.y2);
}
static void probe_clock(void) { probe_area("clock", g_clock); }
static void probe_next(void) { probe_area("next", g_next); }
static void probe_number(void) { probe_area("number", C.number[0]); }
/* What LVGL sends to the panel next, as one box around every invalidated area. */
static lv_area_t dirty;
static bool redrawn(void) {
    lv_display_t *d = lv_display_get_default();
    bool any = false;
    for (uint32_t i = 0; i < d->inv_p; i++) {
        if (d->inv_area_joined[i]) continue;
        if (!any) dirty = d->inv_areas[i];
        else dirty = (lv_area_t){LV_MIN(dirty.x1, d->inv_areas[i].x1), LV_MIN(dirty.y1, d->inv_areas[i].y1),
                                 LV_MAX(dirty.x2, d->inv_areas[i].x2), LV_MAX(dirty.y2, d->inv_areas[i].y2)};
        any = true;
    }
    return any;
}
static bool inside(const lv_area_t *a, lv_obj_t *o) {
    lv_area_t b; lv_obj_get_coords(o, &b);
    return a->x1 >= b.x1 && a->y1 >= b.y1 && a->x2 <= b.x2 && a->y2 <= b.y2;
}
static bool minute_ok, same_ok, count_ok, opaque;
/* The minute handler with a new time: only the clock's digits are invalidated, an unchanged next thing nothing. */
static void minute_redraw(void) {
    lv_refr_now(NULL);
    lv_label_set_text(g_clock, "00:00"); lv_refr_now(NULL);
    on_minute(NULL, 0, "time.minute");
    minute_ok = redrawn() && inside(&dirty, g_clock);
    lv_refr_now(NULL);
    uint32_t before = g_revision;
    on_minute(NULL, 0, "time.minute");
    same_ok = !redrawn() && g_revision == before;
}
/* A new message while locked: the message row's number is all that is invalidated. */
static void count_redraw(void) {
    lv_refr_now(NULL);
    ink_notify_t n = {.channel = "message", .title = "合成发信人", .body = "合成短信正文丙", .group = "合成发信人", .prio = -1, .src = INK_SRC_SYSTEM};
    count_ok = ink_notify_post(&n, NULL) == 0;
    fill_rows(false);
    count_ok = count_ok && redrawn() && inside(&dirty, C.number[0]) && !strcmp(C.value[0], "3");
}
static void cover_checks(void) {
    opaque = g_mark && lv_obj_get_width(g_mark) == ink_ui_width() && lv_obj_get_height(g_mark) == ink_ui_height() &&
             lv_obj_get_style_bg_opa(g_mark, 0) == LV_OPA_COVER && !strcmp(ink_ui_top_name(), "LockPage");
}
static int avatars_top, rows_bottom, smallest;
static void geometry(void) {
    lv_area_t r; lv_obj_update_layout(g_mark); lv_obj_get_coords(g_rows, &r); rows_bottom = r.y2;
    avatars_top = ink_ui_height(); smallest = ink_ui_width();
    for (uint32_t i = 0; i < lv_obj_get_child_count(g_mark); i++) {
        lv_obj_t *o = lv_obj_get_child(g_mark, (int32_t)i);
        if (!lv_obj_check_type(o, &lv_button_class)) continue;
        lv_area_t a; lv_obj_get_coords(o, &a);
        avatars_top = LV_MIN(avatars_top, a.y1);
        smallest = LV_MIN(smallest, LV_MIN(lv_area_get_width(&a), lv_area_get_height(&a)));
    }
}
static ink_rc_t ai_rc;
static void ai_through_page(void) { /* the AI takes the action its snapshot offers: the finger's own table */
    ink_ui_page_spec_t page = MODEL; page.object.revision = g_revision; page.actions = C.actions; page.action_count = C.nactions;
    ink_ui_action_target_t target = {ink_ui_content_version(), page.object};
    ink_receipt_t rc;
    ai_rc = ink_ui_semantics_action(&page, "owner", &target, INK_SRC_AI, &rc);
}
static bool primary(const char *initial) { return picture(initial) && hit && ink_ui_surface_kind(hit) == INK_SURFACE_BLACK; }
static void private_text_absent(const char *where) {
    static const char *const SECRET[] = {"合成短信正文", "合成发信人", "00000000002", "星星邮差", "林小满", "山顶的邮筒"};
    for (size_t i = 0; i < sizeof SECRET / sizeof *SECRET; i++)
        CHECK(!shows(SECRET[i]) && !drawn(SECRET[i]), "%s: neither the snapshot nor the cover has %s", where, SECRET[i]);
}
static void privacy(void) {
    lock(); on_ui(cover_checks);
    CHECK(opaque, "the cover is opaque, fills the panel and is the top surface");
    private_text_absent("over the book");
    frame("full");
    wake(); CHECK(BY_HAND("system.home", NULL), "another page under the cover"); quiet();
    lock(); frame("full-home"); /* the runner checks the two pictures are the same */
}
#endif

static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "the space opens"); test_start_home();
    ink_power_hold(INK_NEED_AWAKE); /* keep the driver running while locked, as the other lock tests do */
    if (size != 1) setting("display.font_size", size);
    CHECK(BY_HAND("reader.open_book", "{\"title\":\"星星邮差\"}") && g_rc.status == INK_RC_DONE, "a private page under the lock"); quiet();
#ifndef INK_LOCK_COVER_BASELINE
    if (!strcmp(mode, "reverse")) { seed(); privacy(); ink_probe_count("test.finished"); return; }
#endif
    lock(); frame("empty");
#ifndef INK_LOCK_COVER_BASELINE
    CHECK(C.shown == 0 && !lv_label_get_text(g_next)[0] && !visible("0") && !drawn("条通知"), "nothing unread, nothing next: no rows and no zero");
    CHECK(!shows("\"id\":\"next\"") && !shows("\"id\":\"message\"") && shows("\"id\":\"owner\",\"role\":\"button\""),
          "the empty snapshot has the family, no next thing and no counts: %.200s", snapshot);
    CHECK(visible("我") && visible("小朋友") && visible("已锁定，按电源键或 AI 键唤醒"), "the family and the wake hint are there");
    on_ui(geometry);
    CHECK(smallest >= ink_ui_px(INK_CONTROL), "every picture is at least 108 px to touch (%d)", smallest);
#endif
    wake();
    seed();
#ifdef INK_LOCK_COVER_BASELINE
    lock(); frame("full"); wake();
#else
    privacy();
    CHECK(visible("短信") && visible("未接来电") && visible("15:00 合成接孩子"), "the counts by kind and the next thing are drawn");
    CHECK(shows("\"label\":\"短信\",\"value\":\"2\"") && shows("\"label\":\"未接来电\",\"value\":\"1\"") && shows("15:00 合成接孩子"),
          "the snapshot says the same counts and the next thing: %.200s", snapshot);
    on_ui(geometry);
    CHECK(rows_bottom < avatars_top, "the counts stay above the family (%d < %d)", rows_bottom, avatars_top);
    on_ui(probe_clock); on_ui(probe_next); on_ui(probe_number);
    frame("minute-before");
    ink_time_set(ink_time_now_s() + 60, false); quiet();
    CHECK(visible("14:06"), "the minute ticks on the cover"); frame("minute-after");
    on_ui(minute_redraw);
    CHECK(minute_ok, "a new minute invalidates only the clock's digits (%d,%d-%d,%d)", (int)dirty.x1, (int)dirty.y1, (int)dirty.x2, (int)dirty.y2);
    CHECK(same_ok, "the same minute invalidates nothing and keeps the revision");
    frame("count-before");
    on_ui(count_redraw);
    CHECK(count_ok, "a new message invalidates only the message row's number (%d,%d-%d,%d)", (int)dirty.x1, (int)dirty.y1, (int)dirty.x2, (int)dirty.y2);
    frame("count-after");
    CHECK(shows("\"label\":\"短信\",\"value\":\"3\""), "the snapshot follows the count");
    private_text_absent("after a new message");
    /* Touch and AI: one action table, one capability; the AI is refused and the device stays locked. */
    CHECK(shows("\"id\":\"kid\",\"role\":\"button\"") && !shows("shell.lock_enter"),
          "the snapshot names everyone in the family and offers the AI no way in: %.200s", snapshot);
    CHECK(AI("shell.lock_enter", "{\"id\":\"owner\"}") && g_rc.status != INK_RC_DONE && ink_power_state() == INK_PWR_LOCKED,
          "the AI cannot enter: %s", ink_rc_name(g_rc.status));
    on_ui(ai_through_page);
    CHECK(ai_rc != INK_RC_DONE && ink_power_state() == INK_PWR_LOCKED, "nor through the snapshot's own action (%s)", ink_rc_name(ai_rc));
    CHECK(primary("我") && !primary("小"), "the one whose space it is has the filled picture");
    press_picture("我");
    CHECK(WAIT_FOR(ink_power_state() != INK_PWR_LOCKED, 3000) && ink_member_space_open() && !strcmp(ink_ui_top_name(), "home"),
          "within D-95's two minutes a tap on one's own picture goes straight back (%s)", ink_ui_top_name());
    quiet();
#endif
    CHECK(ink_member_switch("kid") == 0, "the child uses the device"); quiet();
    lock(); frame("child");
#ifndef INK_LOCK_COVER_BASELINE
    CHECK(primary("小") && !primary("我"), "the child's own picture is the filled one");
    CHECK(!visible("短信") && !visible("15:00 合成接孩子") && !shows("合成接孩子"), "the child's cover has nothing of the adult's");
    press_picture("小");
    CHECK(WAIT_FOR(ink_power_state() != INK_PWR_LOCKED, 3000) && ink_member_space_open() && !strcmp(ink_member_current()->id, "kid"),
          "a tap on the child's own picture goes straight back, no PIN");
    quiet();
#else
    wake();
#endif
    CHECK(ink_member_switch("owner") == 0, "back to the adult"); quiet();
    CHECK(BY_HAND("member.set_pin", "{\"id\":\"owner\",\"new_pin\":\"2468\"}") && g_rc.status == INK_RC_DONE, "a synthetic adult PIN");
    CHECK(setting_by_hand("member.idle_return_s", 30), "the shortest lock grace");
    lock();
#ifndef INK_LOCK_COVER_BASELINE
    CHECK(visible("15:00 合成接孩子"), "the next thing shows while the adult's grace lasts");
    on_ui(probe_next); frame("grace");
#endif
    CHECK(WAIT_FOR(!ink_member_space_open(), 40000), "the grace runs out while locked"); quiet();
#ifndef INK_LOCK_COVER_BASELINE
    CHECK(!visible("15:00 合成接孩子") && !shows("合成接孩子"), "a closed space hides the next thing"); frame("closed");
    press_picture("我");
#else
    wake(); on_ui(open_pin); quiet();
#endif
    CHECK(WAIT_FOR(shows("密码内容不可见"), 3000) && !ink_member_space_open(), "the PIN page asks for the adult's PIN (%s)", ink_ui_top_name());
    on_ui(pin_entries);
    CHECK(pin_emergency && pin_camera, "the PIN page has the emergency call (D-119) and the camera (D-121)");
    frame("pin");
    key(HAL_KEY_HOME); quiet();
    CHECK(!ink_member_space_open() && !shows("密码内容不可见"), "leaving the PIN page opens nothing");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_env, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "lock-cover-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the lock cover driver");
}
