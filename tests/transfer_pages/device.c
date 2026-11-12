/* Synthetic HTTP bytes and real transfer, USB, confirmation and file pages. */
#include "../../system/ui/shell/transfer_pages.c"
#include "../services_finish/product.h"
#include "ink/hal_transfer.h"
#include "cJSON.h"
#include <sys/stat.h>

extern int ink_test_transfer_host_mode;
static char scene[32], selected_scene[32], saved_code[8], saved_address[80];
static const char *wanted, *stage;
static int touch_x, touch_y;
static lv_area_t control_bounds;
static unsigned sequence;
static bool acknowledged, large_text;
static void options(void *arg) {
    (void)arg; large_text = getenv("INK_TRANSFER_LARGE") != NULL; const char *s = getenv("INK_TRANSFER_SCENE");
    snprintf(selected_scene, sizeof selected_scene, "%s", s ? s : "Transfer");
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "display settles"); }
#include "../child_app_fixture.h"
static void read_page(void) { read_screen(); snprintf(g_top, sizeof g_top, "%s", ink_ui_page_name()); }
static bool at(const char *name) { on_ui(read_page); return !strcmp(g_top, name); }
static bool ready(const char *name) {
    bool ok = WAIT_FOR(at(name), 12000) && WAIT_FOR(settled(), 15000);
    CHECK(ok, "page %s is presented (actual %s)", name, g_top); return ok;
}
static void locate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) {
        lv_area_t a; lv_obj_get_coords(o, &a); touch_x = (a.x1 + a.x2) / 2; touch_y = (a.y1 + a.y2) / 2;
        lv_obj_get_coords(lv_obj_get_parent(o), &control_bounds);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, (int)i));
}
static void target(void) { touch_x = touch_y = -1; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static void press(const char *label) {
    quiet(); wanted = label; on_ui(target);
    CHECK(touch_x >= 0 && touch_y >= 0, "visible touch control: %s", label);
    if (touch_x >= 0 && touch_y >= 0) tap(touch_x, touch_y);
}
static void command(const char *cap, const char *args) { CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s succeeds (%s)", cap, g_rc.detail); }
static void roots(void) { command("system.home", NULL); command("app.open", "{\"id\":\"files\"}"); ready("Files"); }
static void announce(void *arg) {
    (void)arg; char path[768]; snprintf(path, sizeof path, "%s/session.json", g_sim.out_dir);
    FILE *f = fopen(path, "wb");
    if (!f || fprintf(f, "{\"sequence\":%u,\"stage\":\"%s\",\"session\":%s}\n", sequence, stage, g_result) < 0 || fclose(f)) abort();
    /* Anchor the existing response window after expensive host rendering. */
    vtk_set_realtime_speed(g_sim.realtime_speed);
}
static void ack(void *arg) {
    (void)arg; char path[768]; struct stat st;
    snprintf(path, sizeof path, "%s/ack-%u", g_sim.out_dir, sequence); acknowledged = stat(path, &st) == 0;
}
static void exchange(const char *name) {
    CHECK(BY_HAND("transfer.status", NULL), "human session for external client");
    stage = name; sequence++; acknowledged = false; vtk_host_call(announce, NULL);
    uint64_t end = osal_now_us() + 30000000;
    while (!acknowledged && osal_now_us() < end) { delay_ms(20); vtk_host_call(ack, NULL); }
    CHECK(acknowledged, "external client acknowledged %s", name);
}
static void save_snapshot(void *arg) {
    (void)arg; char path[768]; snprintf(path, sizeof path, "%s/snapshots.ndjson", g_sim.out_dir);
    cJSON *json = cJSON_Parse(g_snap);
    CHECK(json != NULL, "host parser validates the complete snapshot"); cJSON_Delete(json);
    FILE *f = fopen(path, "ab");
    if (!f || fprintf(f, "{\"scene\":\"%s\",\"snapshot\":%s}\n", scene, g_snap) < 0 || fclose(f)) abort();
}
static void inspect(void) {
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, scene), "page fits production AI context: %s", scene);
    if (!strcmp(scene, "Files")) { vtk_host_call(save_snapshot, NULL); return; }
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && !ink_json_parse(j, g_snap, strlen(g_snap)), "snapshot is complete JSON");
    if (j && !ink_json_parse(j, g_snap, strlen(g_snap))) {
        for (int t = 1; t < j->ntok; t++) {
            if (j->tok[t].type != 1) continue;
            char cap[64] = "";
            if (ink_json_str(j, ink_json_find(j, t, "capability"), cap, sizeof cap) > 0)
                CHECK(ink_cap_find(cap) && ink_cap_ai_callable(ink_cap_find(cap)), "button capability is available to AI: %s", cap);
        }
    }
    osal_free(j); vtk_host_call(save_snapshot, NULL);
}
static void frame(void *arg) {
    (void)arg; CHECK(settled(), "capture settled frame");
    ink_probe_count("test.snapshot.settled"); sim_display_snapshot("scene");
}
static void capture(const char *name) {
    if (!ready(name)) return;
    snprintf(scene, sizeof scene, "%s", name); on_ui(inspect);
    char metric[80]; snprintf(metric, sizeof metric, "test.snapshot.%s", name); ink_probe_count(metric);
    bool received_frame = !strcmp(selected_scene, "TransferReceived") && !strcmp(name, "Transfer") && P.received > 0;
    if ((!strcmp(name, selected_scene) && (!P.received || strcmp(name, "Transfer"))) || received_frame) { vtk_event_at(vtk_now() + SIM_MS(1), frame, NULL); delay_ms(10); }
}
static void wireless_contract(void) {
    CHECK(P.active && !strcmp(P.code, saved_code) && !strcmp(P.address, saved_address), "address and digits come from transfer.status");
    wanted = saved_address; target(); CHECK(touch_x >= 0, "visible address equals status host and port");
    wanted = "用手机扫，或在电脑上打开"; target(); CHECK(touch_x >= 0, "wireless instruction matches Transfer design");
    lv_obj_t *qr = NULL;
    for (uint32_t i = 0; i < lv_obj_get_child_count(P.screen); i++)
        if (lv_obj_check_type(lv_obj_get_child(P.screen, (int)i), &lv_qrcode_class)) qr = lv_obj_get_child(P.screen, (int)i);
    CHECK(qr != NULL, "active transfer displays a real LVGL QR code");
    if (qr) {
        lv_draw_buf_t *actual = lv_canvas_get_draw_buf(qr);
        lv_obj_t *expected = lv_qrcode_create(P.screen); lv_obj_add_flag(expected, LV_OBJ_FLAG_HIDDEN);
        lv_qrcode_set_size(expected, (int32_t)actual->header.w);
        lv_qrcode_set_dark_color(expected, lv_color_black()); lv_qrcode_set_light_color(expected, lv_color_white());
        CHECK(lv_qrcode_update(expected, saved_address, (uint32_t)strlen(saved_address)) == LV_RESULT_OK, "encode expected status endpoint");
        lv_draw_buf_t *reference = lv_canvas_get_draw_buf(expected);
        CHECK(actual->data_size == reference->data_size && !memcmp(actual->data, reference->data, actual->data_size), "QR bitmap encodes status host and port");
        lv_obj_delete(expected);
    }
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strstr(g_snap, saved_code), "authorization digits stay out of AI context");
}
static void unavailable_wireless(void) {
    wanted = "还没连上 Wi-Fi"; target(); CHECK(touch_x >= 0, "missing HAL address is visible on page");
    CHECK(!P.address[0] && !strstr(g_snap, "http://"), "missing HAL address exposes no fake URL");
    bool qr = false;
    for (uint32_t i = 0; i < lv_obj_get_child_count(P.screen); i++)
        if (lv_obj_check_type(lv_obj_get_child(P.screen, (int)i), &lv_qrcode_class)) qr = true;
    CHECK(!qr, "missing HAL address exposes no stale QR code");
}
static bool active(void) { return AI("transfer.status", NULL) && strstr(g_result, "\"active\":true"); }
static bool stopped(void) { return AI("transfer.status", NULL) && strstr(g_result, "\"active\":false"); }
static void remember_wireless(void) {
    CHECK(AI("transfer.status", NULL) && strstr(g_result, "\"code\":\"000000\""), "AI status hides authorization digits");
    CHECK(BY_HAND("transfer.status", NULL) && g_rc.status == INK_RC_DONE, "human reads current wireless status");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t port = 0; char host[64] = "", actual[64] = "";
    CHECK(j && !ink_json_parse(j, g_result, strlen(g_result)), "parse service status");
    if (j) {
        ink_json_str(j, ink_json_find(j, 0, "code"), saved_code, sizeof saved_code);
        ink_json_int(j, ink_json_find(j, 0, "port"), &port);
        ink_json_str(j, ink_json_find(j, 0, "host"), host, sizeof host);
    }
    CHECK(!hal_transfer_host(actual, sizeof actual) && host[0] && !strcmp(host, actual), "status host equals read-only HAL address");
    snprintf(saved_address, sizeof saved_address, "http://%s:%u/", host, (unsigned)(uint16_t)port); osal_free(j);
    CHECK(WAIT_FOR(at("Transfer") && strstr(g_snap, saved_address), 5000), "page reads the real endpoint");
}
static void start_wireless(void) {
    press("开始"); CHECK(WAIT_FOR(active(), 5000), "touch starts actual wireless service"); remember_wireless();
}
static void address_changes(void) {
    ink_test_transfer_host_mode = 2; remember_wireless(); on_ui(wireless_contract);
    ink_test_transfer_host_mode = 1;
    CHECK(AI("transfer.status", NULL) && strstr(g_result, "\"host\":\"\"") && strstr(g_result, "\"active\":true"), "HAL failure returns empty status host");
    CHECK(WAIT_FOR(at("Transfer") && strstr(g_snap, "还没连上 Wi-Fi"), 5000), "HAL failure updates the page without a fake endpoint");
    on_ui(unavailable_wireless);
    ink_test_transfer_host_mode = 0; remember_wireless(); on_ui(wireless_contract);
}
static void authorize_usb(void) {
    ink_confirm_view_t v;
    CHECK(WAIT_FOR(ink_confirm_list(&v, 1) == 1, 3000) && !strcmp(v.cap, "usb.mount"), "USB waits for adult confirmation");
    CHECK(!ink_usb_disk_active(), "confirmation has not lent the card");
    CHECK(ink_confirm_decide(v.token, true, INK_SRC_AI) != 0, "AI cannot confirm USB for the adult");
    CHECK(ink_confirm_decide(v.token, true, INK_SRC_TOUCH) == 0, "adult authorizes USB");
}
static unsigned rescans;
static void file_event(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id; char kind[24] = ""; ink_args_get_str(a, "kind", kind, sizeof kind);
    if (!strcmp(kind, "rescan")) rescans++;
}
static void copy_usb(void *arg) {
    (void)arg; char path[768]; snprintf(path, sizeof path, "%s/usb-fixture.txt", g_sim.out_dir);
    sim_usb_copy_in(path, "content/.m/owner/收件箱/SYNTH_USB.txt");
}
static void eject_usb(void *arg) { (void)arg; sim_usb_eject(); }
static double card_writes;
static void sample_writes(void *arg) { (void)arg; sim_storage_report(); card_writes = sim_metric_value("sd.write_bytes"); }
static bool busy_result(void) { return at("TransferUsbBusy") && strstr(g_snap, "有应用在用存储卡"); }
static void run(void *arg) {
    (void)arg; vtk_host_call(options, NULL); delay_ms(4000); test_start_home();
    if (large_text) command("settings.set", "{\"id\":\"display.font_size\",\"value\":3}");
    roots();
    CHECK(strstr(g_snap, "transfer.open") && strstr(g_snap, "transfer.usb"), "adult file home exposes both entries to AI");
    wanted = "1 / 1"; on_ui(target); int pager_bottom = control_bounds.y2;
    CHECK(touch_x >= 0, "file home exposes its pagination");
    wanted = "从手机或电脑传入"; on_ui(target);
    CHECK(touch_x >= 0 && control_bounds.y1 > pager_bottom, "transfer entry clears the file pagination");
    if (!strcmp(selected_scene, "Files")) capture("Files");
    press("从手机或电脑传入"); ready("Transfer"); start_wireless(); on_ui(wireless_contract); capture("Transfer"); address_changes();
    exchange("begin");
    CHECK(WAIT_FOR(at("TransferLive") && strstr(g_snap, "4096 字节"), 5000), "live page shows bytes actually received");
    capture("TransferLive"); exchange("finish");
    CHECK(WAIT_FOR(stopped(), 5000), "completed upload consumes authorization");
    CHECK(WAIT_FOR(at("Transfer") && strstr(g_snap, "已收到 1 个，12288 字节"), 5000), "completion returns to Transfer with actual size and count");
    capture("Transfer");
    press("关闭"); ready("Files"); command("files.browse", "{\"path\":\"/me\"}"); ready("FileList");
    CHECK(WAIT_FOR(at("FileList") && strstr(g_snap, "传进来的"), 10000), "file app labels the real inbox as 传进来的");
    press("传进来的"); ready("FileList");
    CHECK(WAIT_FOR(at("FileList") && strstr(g_snap, "SYNTH_UPLOAD.txt"), 10000), "uploaded file appears in the real file app");
    ink_fs_file_t *f = NULL; char buf[4096]; size_t received = 0; bool same = true; int64_t n = 0;
    CHECK(ink_fs_open("/me/收件箱/SYNTH_UPLOAD.txt", INK_FS_READ, &f) == 0, "uploaded file opens");
    while (f && (n = ink_fs_read(f, buf, sizeof buf)) > 0) {
        for (int64_t i = 0; i < n; i++) if (buf[i] != 'U') same = false;
        received += (size_t)n;
    }
    CHECK(n == 0 && received == 12288 && same, "uploaded content matches every external byte");
    if (f) CHECK(ink_fs_close(f) == 0, "close uploaded file");
    capture("FileList"); roots(); command("transfer.open", NULL); ready("Transfer"); start_wireless();
    command("shell.back", NULL); CHECK(WAIT_FOR(stopped(), 5000), "back closes wireless service");
    command("transfer.open", NULL); ready("Transfer"); start_wireless();
    command("system.home", NULL); CHECK(WAIT_FOR(stopped(), 5000), "HOME closes wireless service");
    roots(); hw(HW_USB_ON, 0, 0);
    CHECK(ink_cap_subscribe("file.changed", file_event, NULL, INK_EXEC_DIRECT) == 0, "observe actual rescan event");
    CHECK(ink_fs_open("/me/收件箱/SYNTH_UPLOAD.txt", INK_FS_READ, &f) == 0, "an application holds the card");
    press("连电脑"); ready("TransferUsbBusy"); press("开始连接"); authorize_usb();
    CHECK(WAIT_FOR(busy_result(), 8000), "held card produces USB busy instead of lending");
    CHECK(!ink_usb_disk_active(), "busy card remains with applications"); capture("TransferUsbBusy");
    CHECK(ink_fs_close(f) == 0, "application stops using the card");
    press("开始连接"); authorize_usb(); capture("TransferUsbOn");
    CHECK(ink_usb_disk_active(), "USB on page reflects actual ownership");
    vtk_event_at(vtk_now() + SIM_MS(1), copy_usb, NULL); delay_ms(20);
    unsigned before = rescans; press("结束传输"); ready("Files");
    CHECK(WAIT_FOR(!ink_usb_disk_active() && rescans > before, 5000), "end returns card and publishes rescan");
    ink_fs_stat_t st; CHECK(ink_fs_stat("/me/收件箱/SYNTH_USB.txt", &st) == 0 && st.size == 13, "computer's new file is readable after end");
    command("transfer.usb", NULL); ready("TransferUsbBusy");
    CHECK(AI("usb.mount", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI uses same USB capability"); authorize_usb(); ready("TransferUsbOn");
    before = rescans; vtk_event_at(vtk_now() + SIM_MS(1), eject_usb, NULL);
    CHECK(WAIT_FOR(!ink_usb_disk_active() && rescans > before, 5000), "computer eject also returns and rescans card"); ready("Files");
    command("transfer.usb", NULL); ready("TransferUsbBusy"); press("开始连接"); authorize_usb(); ready("TransferUsbOn");
    CHECK(WAIT_FOR(ink_db_idle(), 10000), "index writes finish before bad-card sample");
    vtk_host_call(sample_writes, NULL); double before_writes = card_writes;
    exchange("bad-before"); hw(HW_USB_OFF, 0, 0); capture("TransferUsbFail"); exchange("bad-after");
    vtk_host_call(sample_writes, NULL);
    CHECK(card_writes == before_writes, "bad card causes zero SD write bytes and no formatting");
    CHECK(!ink_usb_disk_active() && ink_fs_stat("/me/收件箱/SYNTH_UPLOAD.txt", &st) == INK_FS_ERR_NO_CARD, "bad card fails honestly");
    press("知道了"); ready("Files");
    permit_child_files();
    CHECK(ink_member_switch("kid") == 0, "switch to child"); roots();
    CHECK(!strstr(g_snap, "transfer.open") && !strstr(g_snap, "transfer.usb"), "child sees neither entry");
    wanted = "从手机或电脑传入"; on_ui(target); CHECK(touch_x < 0, "child has no wireless touch entry");
    wanted = "连电脑"; on_ui(target); CHECK(touch_x < 0, "child has no USB touch entry");
    CHECK(AI("transfer.open", NULL) && g_rc.status == INK_RC_REJECTED, "child direct wireless navigation is rejected");
    CHECK(AI("transfer.start", NULL) && g_rc.status == INK_RC_REJECTED, "child AI cannot start wireless transfer");
    CHECK(BY_HAND("transfer.usb", NULL) && g_rc.status == INK_RC_REJECTED, "child direct USB navigation is rejected");
    CHECK(AI("usb.mount", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot lend entire card");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); osal_task_cfg_t cfg = {.name = "transfer-pages-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start transfer page scenario");
}
