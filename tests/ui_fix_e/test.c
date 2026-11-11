/* Synthetic UX09/UX15/UX18 scenarios against the real product and capabilities. */
#include "ink/sys/file_picker.h"
#include "ink/sys/ai.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "image_fixtures.h"
#include <stdlib.h>

static bool verify, ready, captured;
static int font_size;
static char snapshot[12000], scene[64];
static const char *wanted;
static lv_obj_t *found;
static unsigned images;
static uint32_t text_id;
static bool picker_error;
int ui_fix_e_index(const char *path, ink_fs_type_t type, uint32_t *cursor, ink_fs_entry_t *entry) {
    return picker_error ? INK_FS_ERR_IO : ink_fs_index(path, type, cursor, entry);
}
#define EXPECT(cond, message) do { if (verify) CHECK(cond, "%s", message); } while (0)
static void options(void *unused) {
    (void)unused;
    verify = getenv("INK_UI_FIX_E_VERIFY") != NULL;
    font_size = getenv("INK_UI_FIX_E_LARGE") ? 3 : 1;
}
static void quiet(void) { CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 15000), "frame settles"); }
static void read_page(void) { CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "read real page snapshot"); }
static void poll(void) {
    read_page(); ready = !strstr(snapshot, "正在读取") && !strstr(snapshot, "正在读");
}
static void wait_page(void) {
    ready = false;
    for (int i = 0; i < 750 && !ready; i++) { on_ui(poll); if (!ready) delay_ms(20); }
    CHECK(ready, "page read completes"); quiet(); on_ui(read_page);
}
static void command(const char *cap, const char *args) {
    CHECK(BY_HAND(cap, args) && g_rc.status == INK_RC_DONE, "%s completes (%d: %.120s)", cap, g_rc.status, g_rc.detail);
    quiet();
}
static void search(lv_obj_t *root) {
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return;
    const char *cap = ink_shell_bound_cap(root);
    if ((cap && !strcmp(cap, wanted)) || (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), wanted))) found = root;
    if (lv_obj_check_type(root, &lv_image_class) && lv_image_get_src(root) && lv_obj_get_width(root) == ink_ui_px(INK_ICON_LIST)) {
        const void *src = lv_image_get_src(root);
        if (lv_image_src_get_type(src) == LV_IMAGE_SRC_VARIABLE && ((const lv_image_dsc_t *)src)->header.cf == LV_COLOR_FORMAT_L8) images++;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) search(lv_obj_get_child(root, (int)i));
}
static void locate(void) { found = NULL; images = 0; lv_obj_update_layout(lv_screen_active()); search(lv_screen_active()); search(lv_layer_sys()); }
static void inspect_empty(void) {
    wanted = "ai.history.clear"; locate(); EXPECT(!found, "UX15 empty history has no callable clear control");
}
static void inspect_entries(void) {
    wanted = "shell.ai_ask"; locate(); lv_obj_t *ask = found;
    wanted = "ai.panel.listen"; locate();
    EXPECT(ask && found && lv_obj_get_height(ask) == lv_obj_get_height(found), "UX15 voice and text have equal control heights");
    if (ask && found) { lv_area_t a, b; lv_obj_get_coords(ask, &a); lv_obj_get_coords(found, &b); EXPECT(a.x2 < b.x1, "UX15 voice and text hit regions do not overlap"); }
}
static void host_snapshot(void *unused) {
    (void)unused; char path[768]; int n = snprintf(path, sizeof path, "%s/snapshots.ndjson", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "ab");
    if (!f || fprintf(f, "{\"scene\":\"%s\",\"snapshot\":%s}\n", scene, snapshot) < 0 || fclose(f)) abort();
}
static void frame(void *unused) { (void)unused; sim_display_snapshot(scene); captured = true; }
static void capture(const char *name) {
    quiet(); snprintf(scene, sizeof scene, "%s", name); on_ui(read_page); vtk_host_call(host_snapshot, NULL);
    captured = false; vtk_event_at(vtk_now() + SIM_MS(1), frame, NULL);
    CHECK(WAIT_FOR(captured, 1000), "capture %s", name);
}
static void home(void) { command("system.home", NULL); }
static void chosen(void *ctx, uint32_t id) { (void)ctx; (void)id; }
static ink_fs_type_t picker_type;
static void open_picker(void) { CHECK(!ink_file_picker_open(picker_type, chosen, NULL), "open real file picker"); }
static void inspect_images(void) { wanted = "SYNTH_PICKER"; locate(); ready = images >= 1; }
static void family(void) {
    command("shell.family", NULL); capture("family");
    command("shell.records", "{\"member\":\"kid\"}"); wait_page(); capture("records");
    EXPECT(strstr(snapshot, "删除这位家人的活动记录") && strstr(snapshot, "孩子"), "UX09 deletion names its scope and selected family member");
    command("shell.kid_settings", NULL); command("family.kid_apps", NULL); capture("kid-apps");
    EXPECT(strstr(snapshot, "始终可用"), "UX09 required communication is read-only and always available");
    command("shell.back", NULL); command("family.kid_periods", NULL); capture("kid-periods");
    EXPECT(strstr(snapshot, "开始—结束") && strstr(snapshot, "次日") && strstr(snapshot, "保存时段"), "UX09 start and end share one continuous editor with an overnight summary");
    if (verify && strstr(snapshot, "time_editor")) {
        command("family.kid_periods", "{\"edit\":\"focus\"}");
        command("text.select", "{\"start\":0,\"end\":2147483647}"); command("text.insert", "{\"text\":\"22:15-06:45\"}");
        CHECK(BY_HAND("family.kid_periods", "{\"edit\":\"save\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "period editor uses the existing parent confirmation");
        ink_confirm_view_t v; CHECK(ink_confirm_list(&v, 1) == 1, "period save presents one confirmation");
        CHECK(ink_confirm_accept_with_pin(v.token, "2468", NULL, NULL) == 0, "person confirms the period with the synthetic parent PIN");
        ink_kid_period_t p;
        bool saved = false;
        for (int i = 0; i < 500 && !saved; i++) { ink_member_period("kid", 0, &p); saved = p.start == 1335 && p.end == 405; if (!saved) delay_ms(20); }
        CHECK(saved, "one save writes both actual times, with no half-hour cycling");
        quiet(); capture("kid-periods-saved");
    }
    home();
}
static void history(void) {
    command("shell.ai_home", NULL); wait_page(); on_ui(inspect_empty); on_ui(inspect_entries); capture("history-empty"); home();
    ink_ai_history_save("owner", "合成问题：为什么月亮会有不同的形状，请解释一下", "合成回答", "ReaderSynthetic");
    CHECK(AI("ai.history.list", NULL) && g_rc.status == INK_RC_DONE, "read completed synthetic history");
    command("shell.ai_home", NULL); wait_page(); capture("history");
    EXPECT(strstr(snapshot, "阅读器") && !strstr(snapshot, "ReaderSynthetic") && strstr(snapshot, "请解释一下"), "UX15 question and translated source are distinguishable");
    on_ui(inspect_entries); home();
}
static void files(void) {
    CHECK(!ink_fs_mkdir("/me/SYNTH_UI_EMPTY"), "create an empty synthetic folder");
    CHECK(!ink_fs_write_atomic("/me/SYNTH_UI/合成笔记.txt", "synthetic", 9), "write synthetic text");
    uint32_t cursor = 0; ink_fs_entry_t e;
    while (ink_fs_index("/me/SYNTH_UI", INK_FS_TEXT, &cursor, &e) == 1) if (strstr(e.path, "合成笔记")) text_id = e.id;
    CHECK(text_id != 0, "synthetic file has a stable id");
    command("app.open", "{\"id\":\"files\"}"); capture("files");
    command("files.browse", "{\"path\":\"/me/SYNTH_UI\"}"); wait_page();
    command("files.new", "{\"text\":true}"); capture("file-name"); command("files.up", NULL); wait_page();
    command("files.multi", NULL); wait_page(); capture("files-multi-empty");
    EXPECT(!strstr(snapshot, "\"capability\":\"files.move\""), "UX18 empty selection has no executable copy or move toolbar action");
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", text_id); command("files.mark", args); wait_page(); capture("files-multi");
    EXPECT(strstr(snapshot, "已选 1 个文件") && strstr(snapshot, "已选择"), "UX18 multi-selection shows a count and marked state");
    command("files.move", "{\"copy\":true}"); wait_page(); capture("files-copy");
    home(); command("app.open", "{\"id\":\"files\"}"); command("files.browse", "{\"path\":\"/me/SYNTH_UI\"}"); wait_page();
    command("files.select", args); wait_page(); capture("file-actions");
    command("files.edit", NULL); wait_page(); capture("file-edit");
    home(); command("app.open", "{\"id\":\"files\"}"); command("files.browse", "{\"path\":\"/me/SYNTH_UI\"}"); wait_page(); command("files.select", args); wait_page();
    command("files.move", NULL); wait_page(); capture("files-move");
    wanted = "移动：合成笔记.txt"; on_ui(locate); EXPECT(found, "UX18 move identifies its source file");
    home(); command("app.open", "{\"id\":\"files\"}"); command("files.browse", "{\"path\":\"/me/SYNTH_UI_EMPTY\"}"); wait_page(); capture("files-empty");
    command("files.browse", "{\"path\":\"/me/SYNTH_MISSING\"}"); wait_page(); capture("files-error");
    EXPECT(strstr(snapshot, "重新读取") && strstr(snapshot, "暂时读不了"), "UX18 failure retains location and exposes manual reread");
    home();
}
static void picker(void) {
    CHECK(!ink_fs_write_atomic("/family/SYNTH_PICKER.png", color_png, sizeof color_png), "write a real synthetic image");
    picker_type = INK_FS_IMAGE; on_ui(open_picker); wait_page();
    if (verify) {
        ready = false;
        for (int i = 0; i < 500 && !ready; i++) { on_ui(inspect_images); if (!ready) delay_ms(20); }
        CHECK(ready, "UX18 picker renders the existing service thumbnail");
    }
    capture("picker-images"); command("file_picker.cancel", NULL);
    picker_type = INK_FS_AUDIO; on_ui(open_picker); wait_page(); capture("picker-empty");
    wanted = "没有符合条件的文件"; on_ui(locate);
    EXPECT(found && strstr(snapshot, "\"total\":0") && !strstr(snapshot, "暂时读不了"), "UX18 unmatched type is an empty state");
    command("file_picker.cancel", NULL);
    picker_error = true; on_ui(open_picker); wait_page(); capture("picker-error");
    EXPECT(strstr(snapshot, "暂时读不了文件"), "UX18 picker read failure is reported as an error");
    wanted = "没有符合条件的文件"; on_ui(locate); EXPECT(!found, "UX18 read failure does not display the empty-state label");
    command("file_picker.cancel", NULL); picker_error = false;
}
static void run(void *unused) {
    (void)unused; vtk_host_call(options, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "owner space opens"); test_start_home(); quiet();
    char args[96]; snprintf(args, sizeof args, "{\"id\":\"display.font_size\",\"value\":%d}", font_size); command("settings.set", args);
    if (verify) command("member.set_pin", "{\"id\":\"owner\",\"new_pin\":\"2468\"}");
    family(); history(); files(); picker(); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); osal_task_cfg_t cfg = {.name = "ui-fix-e-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start UX09/UX15/UX18 test");
}
