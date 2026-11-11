#ifndef UI_FIX_PRODUCT
#define UI_FIX_PRODUCT "../../system/apps/camera/scan.c"
#endif
#include UI_FIX_PRODUCT
#include "common.h"

static char payload[301], collected[601];
static bool failed;
static void inject(void) {
    scan_t *p = osal_calloc(1, sizeof *p, OSAL_MEM_EXTERNAL);
    CHECK(p != NULL, "allocate synthetic decoded result");
    if (!p) return;
    p->ok = !failed && parse(p, payload); p->epoch = ink_member_epoch();
    if (!strcmp(ink_ui_page_name(), "Scan")) {
        lv_obj_t *root = scan_root; destroy(); scan_root = root;
    } else ink_ui_push(&SCAN, NULL);
    result = p; lv_obj_clean(scan_root); create(scan_root, NULL); ink_ui_content_changed(); ink_ui_hint_obj(scan_root, INK_REFRESH_UI);
}
static void url_check(void) {
    CHECK(fx_bound(scan_root, "clipboard.write"), "V07 URL keeps its copy action");
    CHECK(!fx_label(scan_root, "设备没有浏览器"), "V07 never claims the device has no browser");
    CHECK(fx_bound(scan_root, "remote.open"), "V07 URL opens through the existing webpage capability");
}
static void long_check(void) {
    lv_obj_t *label = fx_label(scan_root, "测试行");
    if (!label) label = fx_label(scan_root, "结束标记");
    CHECK(label, "V07 complete page text exists");
    if (!label) return;
    CHECK(!lv_obj_has_flag(lv_obj_get_parent(label), LV_OBJ_FLAG_SCROLLABLE), "V07 result is static, never continuously scrollable");
    fx_fits(label, lv_obj_get_parent(label));
    size_t have = strlen(collected), n = strlen(lv_label_get_text(label));
    if (have + n < sizeof collected) memcpy(collected + have, lv_label_get_text(label), n + 1);
}
static void retry_check(void) { CHECK(fx_bound(scan_root, "camera.scan"), "V06 failure preserves manual rescan"); }
static void privacy_check(void) {
    CHECK(!fx_label(scan_root, "是否执行下面的操作"), "V08 names the action instead of a universal confirmation question");
}
static void limited_check(void) {
    CHECK(!fx_bound(scan_root, "remote.open"), "V07 unavailable web action is absent");
    CHECK(fx_bound(scan_root, "clipboard.write") && fx_label(scan_root, "当前无法打开此网址"), "V07 explains the restriction and keeps copy");
}
static void run(void *ctx) {
    (void)ctx; fx_start();
    snprintf(payload, sizeof payload, "https://example.com/qr"); on_ui(inject); fx_capture("scan-url");
    if (!fx_before) on_ui(url_check);
    fx_press("复制"); char clip[301];
    CHECK(ink_clip_get(clip, sizeof clip, INK_SRC_TOUCH, NULL) > 0 && !strcmp(clip, payload), "V07 copy preserves the entire URL");
    if (!fx_before) {
        fx_press("打开网页"); CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "remote"), 5000), "V07 URL button enters real remote app without live internet");
    }
    snprintf(payload, sizeof payload, "WIFI:T:WPA;S:SYNTH-network;P:SYNTH-secret;;"); on_ui(inject); fx_capture("scan-wifi");
    if (!fx_before) on_ui(privacy_check);
    CHECK(!strstr(fx_snapshot, "SYNTH-secret"), "V08 AI snapshot excludes Wi-Fi password");
    snprintf(payload, sizeof payload, "MECARD:N:测试联系人;TEL:10000000000;;"); on_ui(inject); fx_capture("scan-contact");
    snprintf(payload, sizeof payload, "tel:10000000000"); on_ui(inject); fx_capture("scan-tel");
    payload[0] = 0;
    for (int i = 0; i < 27; i++) strcat(payload, "测试行\n");
    strcat(payload, "结束标记"); on_ui(inject); fx_capture("scan-long-first");
    if (!fx_before) {
        collected[0] = 0; on_ui(long_check);
        for (int i = 0; i < 30 && ink_cap_find("camera.scan_next"); i++) {
            if (!AI("camera.scan_next", NULL) || g_rc.status != INK_RC_DONE) break;
            fx_quiet(); on_ui(long_check);
        }
        CHECK(!strcmp(collected, payload), "V07 page concatenation preserves every UTF-8 byte and newline");
        fx_capture("scan-long-last");
        CHECK(AI("camera.scan_prev", NULL) && g_rc.status == INK_RC_DONE, "V07 AI can turn the same pages");
    }
    failed = true; on_ui(inject); fx_capture("scan-failed"); on_ui(retry_check);
    fx_press("再扫一次"); CHECK(WAIT_FOR(!__atomic_load_n(&scanning, __ATOMIC_ACQUIRE), 10000), "V06 retry finishes on existing MEDIA executor");
    fx_quiet(); on_ui(retry_check);
    failed = false; CHECK(ink_member_switch("kid") == 0, "enter child space"); fx_quiet();
    snprintf(payload, sizeof payload, "https://example.com/qr"); on_ui(inject); fx_capture("scan-limited");
    if (!fx_before) on_ui(limited_check);
    ink_probe_count("test.finished");
}
FX_MAIN("ui-fix-g-scan")
