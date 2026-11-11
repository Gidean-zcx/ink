/* Real backup UI and services, synthetic member files and peer faults. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

void sim_backup_faults(unsigned faults);

static const char *wanted, *picture;
static int x, y;
static bool captured;
static char context[6144], preview[33];
static void locate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) {
        lv_area_t a; lv_obj_get_coords(o, &a);
        x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, (int32_t)i));
}
static void find_label(void) {
    x = y = -1; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active());
}
static void press(const char *label) {
    wanted = label; on_ui(find_label);
    CHECK(x >= 0 && y >= 0, "actual touch control: %s", label);
    if (x >= 0 && y >= 0) tap(x, y);
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void read_page(void) { read_screen(); snprintf(g_top, sizeof g_top, "%s", ink_ui_page_name()); }
static bool at(const char *name) { on_ui(read_page); return !strcmp(g_top, name); }
static bool ready(const char *name) {
    bool ok = WAIT_FOR(at(name), 12000) && WAIT_FOR(settled(), 10000);
    CHECK(ok, "page %s is presented (actual %s)", name, g_top); return ok;
}
static bool phase(const char *name) {
    return AI("backup.status", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, name);
}
static void inspect(void) {
    const char *name = NULL;
    CHECK(ink_ui_snapshot(context, sizeof context, &name) > 0 && name && !strcmp(name, picture),
        "all six pages expose a production AI snapshot: %s", picture);
    CHECK(ink_ui_context_snapshot(context, sizeof context, NULL) > 0,
        "page fits the real AI context buffer: %s", picture);
    CHECK(!strstr(context, "测试孩子私有") && !strstr(context, "SAVED") && !strstr(context, "LOCAL"),
        "page shows only current member summary, never file contents");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && !ink_json_parse(j, g_snap, strlen(g_snap)), "page snapshot is complete JSON");
    if (j && !ink_json_parse(j, g_snap, strlen(g_snap))) {
        int actions = ink_json_find(j, 0, "actions");
        CHECK(actions >= 0, "page declares its actions");
        for (int i = actions + 1; i < j->ntok; i++) {
            if (j->tok[i].type != 1) continue;
            char cap[64];
            if (ink_json_str(j, ink_json_find(j, i, "capability"), cap, sizeof cap) >= 0)
                CHECK(ink_cap_find(cap) && ink_cap_ai_callable(ink_cap_find(cap)), "button and AI use the registered capability %s", cap);
        }
    }
    if (!strcmp(picture, "Restore")) {
        CHECK(strstr(g_snap, "共 ") && !strstr(g_snap, "正在查找"), "large manifest produces a usable summary preview");
        CHECK(strstr(g_snap, "restore.apply") && strstr(g_snap, "不重放"), "preview declares overwrite and omitted content");
    }
    if (!strcmp(picture, "RestoreFail")) CHECK(strstr(g_snap, "说明"), "restore failure explanation remains available to AI");
    osal_free(j);
}
static void capture_event(void *arg) {
    CHECK(settled(), "capture a fully presented frame"); sim_display_snapshot(arg); captured = true;
}
static void capture(const char *name) {
    if (!ready(name)) return;
    picture = name; on_ui(read_page); on_ui(inspect);
    captured = false; vtk_event_at(vtk_now() + SIM_MS(1), capture_event, (void *)name);
    CHECK(WAIT_FOR(captured, 2000), "captured %s", name);
    char metric[80]; snprintf(metric, sizeof metric, "test.snapshot.%s", name); ink_probe_count(metric);
}
static void card_event(void *arg) { sim_sd_set_present((intptr_t)arg != 0); }
static void outage(void *arg) { (void)arg; sim_net_set_link("none"); }
static void reconnect(void *arg) { (void)arg; sim_net_set_link("wifi"); }
static bool online(void) {
    int64_t v = 0; ink_state_get_int(ink_state_find("net.online"), &v); return v != 0;
}
static void corrupt_event(void *arg) { (void)arg; sim_backup_faults(2); }
static void clear_fault(void *arg) { (void)arg; sim_backup_faults(0); }
static void fixture(void) {
    CHECK(ink_fs_write_atomic("/me/page-note.txt", "SAVED", 5) == INK_FS_OK, "synthetic original file");
    for (int i = 0; i < 24; i++) {
        char path[80]; snprintf(path, sizeof path, "/me/test-only-%02d.txt", i);
        CHECK(ink_fs_write_atomic(path, "fixture", 7) == INK_FS_OK, "many real manifest entries");
    }
    size_t size = 2 * 1024 * 1024;
    char *buf = osal_malloc(size, OSAL_MEM_EXTERNAL);
    CHECK(buf, "bounded external test buffer");
    if (buf) {
        memset(buf, 'T', size);
        CHECK(ink_fs_write_atomic("/me/test-large.txt", buf, size) == INK_FS_OK, "real hashing and restore work");
        osal_free(buf);
    }
}
static void check_file(const char *expected) {
    char buf[16] = {0}; ink_fs_file_t *f = NULL;
    CHECK(ink_fs_open("/me/page-note.txt", INK_FS_READ, &f) == INK_FS_OK, "open the member file");
    if (f) {
        CHECK(ink_fs_read(f, buf, sizeof buf) == 5 && !memcmp(buf, expected, 5), "restore result is actual file content: %s", expected);
        CHECK(ink_fs_close(f) == INK_FS_OK, "close member file");
    }
}
static void get_preview(void) {
    CHECK(AI("backup.status", NULL) && strstr(g_result, "\"phase\":\"preview\""), "real service has a preview");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && !ink_json_parse(j, g_result, strlen(g_result)) &&
        ink_json_str(j, ink_json_find(j, 0, "preview"), preview, sizeof preview) == 32, "preview is bound by a real token");
    osal_free(j);
}
static unsigned confirmation(void) {
    ink_confirm_view_t view;
    CHECK(WAIT_FOR(ink_confirm_list(&view, 1) == 1, 3000) && !strcmp(view.cap, "restore.apply"), "restore uses existing overwrite confirmation");
    CHECK(ink_confirm_decide(view.token, true, INK_SRC_AI) != 0, "AI cannot approve destructive overwrite");
    return view.token;
}
static void run(void *arg) {
    (void)arg; test_start_home();
    CHECK(AI("app.list", NULL) && g_rc.status == INK_RC_DONE && !strstr(g_result, "\"id\":\"backup\""),
        "backup stays in settings, outside the app directory");
    CHECK(AI("shell.storage", NULL) && g_rc.status == INK_RC_DONE && ready("SetStorage"), "open storage settings");
    CHECK(strstr(g_snap, "\"capability\":\"backup.open\"") && !strstr(g_snap, "\"capability\":\"backup.now\""),
        "storage row declares backup.open for both touch and AI");
    press("备份");
    CHECK(ready("Backup"), "empty backup status is reachable");
    CHECK(phase("\"phase\":\"none\""), "opening the storage row does not start a backup");
    CHECK(AI("shell.back", NULL) && ready("SetStorage"), "backup returns to storage settings");
    CHECK(AI("backup.open", NULL) && g_rc.status == INK_RC_DONE && ready("Backup"), "AI opens the same backup page");
    CHECK(AI("backup.open", NULL) && g_rc.status == INK_RC_DONE && ready("Backup"), "AI can reopen backup without stacking pages");
    CHECK(phase("\"phase\":\"none\""), "AI navigation does not start a backup");
    CHECK(AI("shell.back", NULL) && ready("SetStorage"), "reopening still returns directly to storage settings");
    const char *probe = getenv("INK_BACKUP_PAGE_PROBE");
    if (probe && !strcmp(probe, "navigation")) { ink_probe_count("test.finished"); return; }
    press("备份"); CHECK(ready("Backup"), "reenter backup from storage settings");
    fixture();
    vtk_event_at(vtk_now() + SIM_MS(1), card_event, NULL); delay_ms(50);
    press("现在备份一次"); capture("BackupFail");
    CHECK(strstr(g_snap, "存储卡"), "backup failure reports the actual missing card");
    CHECK(AI("shell.back", NULL) && ready("SetStorage"), "backup failure returns to storage settings");
    vtk_event_at(vtk_now() + SIM_MS(1), card_event, (void *)1); delay_ms(50);
    CHECK(AI("backup.open", NULL) && ready("BackupFail"), "backup failure remains visible when reopened");
    press("再试一次");
    CHECK(WAIT_FOR(phase("\"phase\":\"saved\""), 180000), "touch actually backs up files to the synthetic peer");
    CHECK(WAIT_FOR(at("Backup") && strstr(g_snap, "已备份到家庭服务器"), 5000), "saved service state updates the UI");
    capture("Backup");
    CHECK(ink_fs_write_atomic("/me/page-note.txt", "LOCAL", 5) == INK_FS_OK, "modify the current file after backup");
    press("恢复备份");
    CHECK(WAIT_FOR(phase("\"phase\":\"preview\""), 180000), "touch loads the latest backup preview");
    CHECK(WAIT_FOR(at("Restore") && strstr(g_snap, "restore.apply"), 5000), "summary enables the actual overwrite button");
    capture("Restore"); get_preview();
    if (probe && !strcmp(probe, "summary")) { ink_probe_count("test.finished"); return; }
    if (probe && !strcmp(probe, "routing")) {
        CHECK(AI("backup.now", NULL) && g_rc.status == INK_RC_QUEUED, "AI can start a backup from restore preview");
        CHECK(ready("Backup"), "AI backup replaces a restore preview with Backup");
        ink_probe_count("test.finished"); return;
    }
    CHECK(AI("backup.manifest", NULL) && g_rc.status == INK_RC_DONE, "large full manifest exists");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && ink_json_parse(j, g_result, strlen(g_result)) != 0, "full manifest exceeds the UI JSON reader");
    CHECK(AI("backup.manifest", "{\"summary\":true}") && g_rc.status == INK_RC_DONE &&
        j && !ink_json_parse(j, g_result, strlen(g_result)) && strstr(g_result, "item_count"), "summary fits that same reader");
    osal_free(j);
    press("恢复备份"); unsigned token = confirmation();
    CHECK(ink_confirm_decide(token, false, INK_SRC_TOUCH) == 0, "person cancels overwrite");
    CHECK(ready("Restore"), "cancelling confirmation returns to preview"); check_file("LOCAL");
    char args[80]; snprintf(args, sizeof args, "{\"preview\":\"%s\"}", preview);
    CHECK(AI("restore.apply", args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI uses the same restore.apply capability as the button");
    token = confirmation();
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "person authorizes AI-requested restore");
    capture("RestoreRun");
    CHECK(WAIT_FOR(phase("\"phase\":\"done\""), 180000), "real service restores the backup");
    capture("RestoreDone"); check_file("SAVED");
    CHECK(strstr(g_snap, "system.reboot"), "completed restore offers reboot rather than claiming the data is already active");
    CHECK(AI("system.home", NULL) && ready("home"), "test leaves completion without executing its reboot action");
    CHECK(AI("backup.open", NULL) && ready("Backup"), "reopen after restore");
    press("恢复备份");
    CHECK(WAIT_FOR(phase("\"phase\":\"preview\""), 180000) && ready("Restore"), "reenter preview before the next AI backup");
    CHECK(AI("backup.now", NULL) && g_rc.status == INK_RC_QUEUED, "AI can start a backup from restore preview");
    CHECK(ready("Backup"), "AI backup replaces a restore preview with Backup");
    CHECK(WAIT_FOR(phase("\"phase\":\"saved\""), 180000), "AI-requested backup actually finishes");
    vtk_event_at(vtk_now() + SIM_MS(1), corrupt_event, NULL); delay_ms(10);
    CHECK(AI("restore.preview", "{\"summary\":true}") && g_rc.status == INK_RC_QUEUED, "AI preview uses the same summary capability as touch");
    CHECK(WAIT_FOR(phase("\"phase\":\"failed\""), 30000), "corrupted backup is rejected");
    capture("RestoreFail"); check_file("SAVED");
    press("先不恢复"); CHECK(ready("home"), "restore failure returns home");
    vtk_event_at(vtk_now() + SIM_MS(1), clear_fault, NULL); delay_ms(10);
    CHECK(AI("backup.open", NULL) && ready("BackupFail"), "failed operation remains visible");
    press("再试一次");
    CHECK(WAIT_FOR(phase("\"phase\":\"saved\""), 180000), "backup failure retry reaches saved again");
    vtk_event_at(vtk_now() + SIM_MS(1), outage, NULL);
    CHECK(WAIT_FOR(!online(), 3000), "network fixture is actually offline");
    CHECK(AI("backup.now", NULL) && g_rc.status == INK_RC_QUEUED, "AI starts the same backup.now operation offline");
    CHECK(WAIT_FOR(at("BackupFail") && strstr(g_snap, "连不上家庭服务器"), 5000), "offline backup branch is reachable");
    CHECK(AI("shell.back", NULL) && ready("home"), "offline page can return without cancelling service work");
    vtk_event_at(vtk_now() + SIM_MS(1), reconnect, NULL);
    CHECK(WAIT_FOR(phase("\"phase\":\"saved\""), 180000), "existing service resumes after network recovery");
    CHECK(ink_member_switch("kid") == 0, "switch to synthetic child");
    CHECK(AI("backup.open", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot enter adult backup pages");
    CHECK(BY_HAND("backup.open", NULL) && g_rc.status == INK_RC_REJECTED, "child touch cannot enter adult backup pages");
    CHECK(ink_fs_write_atomic("/me/child-only.txt", "测试孩子私有", strlen("测试孩子私有")) == INK_FS_OK, "child owns separate fixture");
    CHECK(ink_member_switch("owner") == 0, "return to adult");
    CHECK(AI("backup.open", NULL) && ready("Backup"), "new member incarnation reopens cleanly");
    picture = "Backup"; on_ui(read_page); on_ui(inspect);
    ink_probe_count("test.snapshot.settled"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "backup-pages-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start bounded synthetic UI driver");
}
