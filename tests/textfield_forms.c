/* Capture real form presenters with synthetic data at both text sizes. */
#include <stdlib.h>
#ifndef INK_TEST_TEXTFIELD_SOURCE
#define INK_TEST_TEXTFIELD_SOURCE "../system/ui/textfield.c"
#endif
#ifndef INK_TEST_REMOTE_SOURCE
#define INK_TEST_REMOTE_SOURCE "../system/apps/remote/remote.c"
#endif
#include INK_TEST_TEXTFIELD_SOURCE
#include INK_TEST_REMOTE_SOURCE
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include "ink/sys/file.h"

static bool captured;
static const char *picture;
static int size_level;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "form settles"); }
static void command(const char *cap, const char *args) {
    CHECK(BY_HAND(cap, args) && g_rc.status == INK_RC_DONE, "%.40s: %d %.100s", cap, g_rc.status, g_rc.say);
    quiet();
}
static void home(void) { command("system.home", NULL); }
#ifndef INK_TEST_BASELINE
enum { FIELD_KINDS = INK_FIELD_PASSWORD + 1 };
static int seen_heights[FIELD_KINDS];
static unsigned seen_fields;
static void inspect_fields(lv_obj_t *obj) {
    for (uint32_t i = 0; i < lv_obj_get_event_count(obj); i++) {
        lv_event_dsc_t *event = lv_obj_get_event_dsc(obj, i);
        if (lv_event_dsc_get_cb(event) != field_event) continue;
        seen_fields++;
        field_t *f = lv_event_dsc_get_user_data(event);
        lv_area_t title, area; lv_obj_get_coords(f->title, &title); lv_obj_get_coords(f->area, &area);
        CHECK(lv_obj_is_visible(f->title) && title.y2 < area.y1, "%s keeps its label above the input", picture);
        CHECK(title.x1 >= 0 && title.x2 < ink_ui_width() && area.x1 >= 0 && area.x2 < ink_ui_width(), "%s input and label fit horizontally", picture);
        if (ink_text_focused()) {
            lv_area_t keyboard; ink_shell_keyboard_area(&keyboard);
            CHECK(area.y2 < keyboard.y1 && title.y1 >= 0, "%s input and label stay above the keyboard", picture);
        }
        lv_point_t measured;
        lv_text_get_size(&measured, lv_label_get_text(f->title), lv_obj_get_style_text_font(f->title, 0), 0, 0, lv_obj_get_width(f->title), LV_TEXT_FLAG_NONE);
        CHECK(measured.y <= lv_area_get_height(&title), "%s label has no vertical clipping", picture);
        if (!f->cfg.draft_page) {
            int h = lv_area_get_height(&area);
            CHECK(!seen_heights[f->kind] || seen_heights[f->kind] == h, "%s has equal heights for the same input kind", picture);
            seen_heights[f->kind] = h;
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) inspect_fields(lv_obj_get_child(obj, (int32_t)i));
}
static void inspect_page(void) {
    memset(seen_heights, 0, sizeof seen_heights); seen_fields = 0;
    lv_obj_update_layout(lv_screen_active()); inspect_fields(lv_screen_active());
    CHECK(seen_fields > 0, "%s contains a real input control", picture);
}
#endif
static void frame(void *ctx) { sim_display_snapshot(ctx); captured = true; }
static void capture(const char *name) {
    quiet(); picture = name; captured = false;
#ifndef INK_TEST_BASELINE
    on_ui(inspect_page);
#endif
    vtk_event_at(vtk_now() + SIM_MS(1), frame, (void *)picture);
    CHECK(WAIT_FOR(captured, 2000), "form captured: %s", name);
}
static void apps(void) { ink_shell_home_swipe(INK_SWIPE_LEFT); }
static void remote_type(void) { RS.secret = false; page_show(TYPE); }
static void remote_secret(void) { RS.secret = true; page_show(TYPE); }
static void choose_size(void *ctx) { (void)ctx; size_level = getenv("INK_FIELD_BIG") ? 3 : 1; }
static void environment(void *ctx) { (void)ctx; sim_wifi_ap_add("test-wifi", -42, "SYNTH_WIFI_PASSWORD"); }
static lv_obj_t *scan_button;
static void find_scan(lv_obj_t *obj) {
    const char *cap = ink_shell_bound_cap(obj);
    if (cap && !strcmp(cap, "wifi.scan")) scan_button = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) find_scan(lv_obj_get_child(obj, (int32_t)i));
}
static void scan(void) {
    scan_button = NULL; find_scan(lv_screen_active());
    if (scan_button) lv_obj_send_event(scan_button, LV_EVENT_CLICKED, NULL);
}
static bool scanned(void) { on_ui(read_screen); return strstr(g_snap, "test-wifi") != NULL; }
static void network_name(void) { ink_text_commit_utf8("SYNTH_NETWORK"); ink_text_enter(); }
static void file_editor(void) {
    ink_fs_entry_t entry; uint32_t cursor = 0, id = 0;
    while (ink_fs_index("/me", INK_FS_ANY, &cursor, &entry) == 1) if (!strcmp(entry.path, "/me/SYNTH_FORM.txt")) id = entry.id;
    CHECK(id != 0, "synthetic text file indexed");
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id);
    command("files.select", args); capture("FileRename"); command("files.edit", NULL); capture("FileEdit");
}
static void run(void *ctx) {
    (void)ctx; vtk_host_call(choose_size, NULL);
    vtk_event_at(vtk_now() + SIM_MS(1), environment, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member opens"); quiet(); test_start_home();
    ink_power_hold(INK_NEED_AWAKE);
    command("settings.set", size_level == 3 ? "{\"id\":\"display.font_size\",\"value\":3}" : "{\"id\":\"display.font_size\",\"value\":1}");
    home(); command("app.open", "{\"id\":\"units\"}"); capture("AppPage");
    home(); command("search.show", "{\"query\":\"\"}"); capture("Search");
    home(); on_ui(apps); capture("Apps");
    home(); command("shell.mobile", NULL); command("shell.mobile_edit", NULL); capture("APN");
    home(); command("shell.ai_ask", NULL); capture("AiAsk");
    home(); command("shell.family", NULL); command("family.edit", NULL); capture("Family");
    home(); command("shell.launch", "{\"id\":\"boot\"}"); command("shell.launch", "{\"id\":\"boot.wifi\"}");
    command("shell.launch", "{\"id\":\"boot.other\"}"); capture("BootName");
    on_ui(network_name); capture("BootPassword");
    home(); command("shell.network", NULL);
    for (int i = 0; i < 4; i++) { on_ui(scan); if (scan_button) break; command("shell.page_next", NULL); }
    CHECK(WAIT_FOR(scanned(), 10000), "real scan callback populates network choices");
    command("shell.wifi_pick", "{\"index\":0}"); capture("WifiPassword");
    home(); command("app.open", "{\"id\":\"upcoming\"}"); command("reminder.new", NULL); capture("Reminder");
    home(); command("app.open", "{\"id\":\"memo\"}"); command("memo.edit", NULL); capture("Memo");
    home(); command("remote.open", NULL); capture("RemoteAddress");
    on_ui(remote_type); capture("RemoteText"); on_ui(remote_secret); capture("RemotePassword");
    home(); command("weather.city", NULL); capture("WeatherCity");
    home(); command("app.open", "{\"id\":\"clock\"}"); command("clock.new", NULL); capture("ClockAlarm");
    home(); command("app.open", "{\"id\":\"files\"}"); command("files.browse", "{\"path\":\"/me\"}"); command("files.new", "{\"text\":true}"); capture("FileName");
    command("files.up", NULL); command("files.new", "{\"text\":false}"); capture("FolderName"); command("files.up", NULL);
    command("file.write", "{\"path\":\"/me/SYNTH_FORM.txt\",\"text\":\"这是合成文本正文，用来核对统一输入框。\"}");
    command("files.browse", "{\"path\":\"/me\"}"); file_editor();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "field-forms", .fn = run, .stack_bytes = 32768, .priority = OSAL_PRIO_NORMAL,
                           .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "form test task");
}
