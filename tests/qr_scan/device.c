/* End-to-end synthetic VGA frames through the real camera HAL and soft bus. */
#include <stdlib.h>
#include <time.h>
#include "quirc.h"
#include "sim.h"
#include "ink/sys/textinput.h"
static struct timespec decode_started;
static void start_clock(void *arg) { (void)arg; if (clock_gettime(CLOCK_MONOTONIC, &decode_started)) abort(); }
static void end_clock(void *arg) {
    (void)arg; struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) abort();
    double us = (now.tv_sec - decode_started.tv_sec) * 1000000.0 + (now.tv_nsec - decode_started.tv_nsec) / 1000.0;
    sim_metric_add("test.qr_decode_us", us); sim_metric_add("test.qr_decode_frames", 1);
}
static void measured_end(struct quirc *q) { vtk_host_call(start_clock, NULL); quirc_end(q); }
static quirc_decode_error_t measured_decode(const struct quirc_code *c, struct quirc_data *d) {
    quirc_decode_error_t r = quirc_decode(c, d); vtk_host_call(end_clock, NULL); return r;
}
#define quirc_end measured_end
#define quirc_decode measured_decode
#include "../../system/apps/camera/scan.c"
#undef quirc_end
#undef quirc_decode
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char fixture_dir[1024], fixture[32];
static lv_point_t button;
static const char *wanted;
static bool located;
static bool source(uint8_t *out, int w, int h, void *ctx) {
    (void)ctx;
    char path[1200];
    int n = snprintf(path, sizeof path, "%s/%s.rgb565", fixture_dir, fixture);
    if (n < 0 || (size_t)n >= sizeof path) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    bool ok = fread(out, 2, (size_t)w * h, f) == (size_t)w * h;
    if (fclose(f)) ok = false;
    return ok;
}
static void options(void *ctx) {
    (void)ctx; const char *path = getenv("INK_QR_IMAGES");
    snprintf(fixture_dir, sizeof fixture_dir, "%s", path ? path : "");
    sim_camera_set_source(source, NULL);
}
static void locate(lv_obj_t *o) {
    const char *id = ink_shell_bound_cap(o);
    if (id && !strcmp(id, wanted) && lv_obj_is_visible(o)) {
        lv_area_t a; lv_obj_get_coords(o, &a); button = (lv_point_t){(a.x1+a.x2)/2, (a.y1+a.y2)/2}; located = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, (int32_t)i));
}
static void find_button(void) { located = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static void touch(const char *cap) {
    wanted = cap; on_ui(find_button); CHECK(located, "visible button bound to %s", cap);
    if (located) tap(button.x, button.y);
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy() && !__atomic_load_n(&scanning, __ATOMIC_ACQUIRE); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "scan and display settle"); }
static void snap(void) {
    ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL);
    CHECK(!strstr(g_snap, "FAKE") && !strstr(g_snap, "password"), "AI snapshot contains no Wi-Fi credential");
}
static void scan_image(const char *name, const char *type, const char *content) {
    snprintf(fixture, sizeof fixture, "%s", name);
    CHECK(AI("camera.scan", NULL), "AI scan returns its asynchronous reply");
    CHECK(g_rc.status == (type ? INK_RC_DONE : INK_RC_FAILED), "scan %s status=%s", name, ink_rc_name(g_rc.status));
    CHECK(strstr(g_result, type ? type : "none") && (!content || strstr(g_result, content)), "scan %s has correct structured type/content: %.150s", name, g_result);
    CHECK(!strstr(g_result, "FAKE") && !strstr(g_rc.say, "FAKE") && !strstr(g_rc.detail, "FAKE"), "AI reply contains no Wi-Fi credential");
    quiet(); on_ui(snap);
    CHECK(!strcmp(ink_ui_page_name(), "Scan"), "scan opens its result page");
}
static int shot_done;
static void screenshot(void *ctx) { sim_display_snapshot(ctx); __atomic_store_n(&shot_done, 1, __ATOMIC_RELEASE); }
static ink_exec_job_t shot;
static void capture(const char *name) {
    quiet(); shot_done = 0; ink_exec_post(INK_EXEC_UI, &shot, screenshot, (void *)name);
    CHECK(WAIT_FOR(__atomic_load_n(&shot_done, __ATOMIC_ACQUIRE), 3000), "screenshot is captured");
}
static void decision(bool yes) {
    ink_confirm_view_t card;
    CHECK(ink_confirm_list(&card, 1) == 1, "result action waits for human approval");
    CHECK(!strstr(card.detail, "FAKE"), "confirmation card excludes password");
    CHECK(ink_confirm_decide(card.token, true, INK_SRC_AI) != 0, "AI cannot replace human approval");
    CHECK(ink_confirm_decide(card.token, yes, INK_SRC_TOUCH) == 0, "human answers scan action");
}
static void run(void *ctx) {
    (void)ctx; CHECK(WAIT_FOR(ink_cap_find("camera.scan") && ink_ui_page_current(), 15000), "system and camera are ready"); test_start_home(); quiet();
    CHECK(ink_cap_find("camera.scan") && ink_cap_find("camera.scan")->child_ok, "scan inherits camera child access");
    snprintf(fixture, sizeof fixture, "url");
    CHECK(BY_HAND("app.open", "{\"id\":\"camera\"}") && g_rc.status == INK_RC_DONE, "open existing camera"); quiet();
    touch("camera.scan");
    CHECK(WAIT_FOR(result && result->ok, 10000), "camera scan button decodes through same handler"); quiet(); capture("camera-scan-zh");
    scan_image("url", "url", "https://example.com/qr"); capture("url-zh");
    touch("clipboard.write");
    char clip[301]; CHECK(ink_clip_get(clip, sizeof clip, INK_SRC_TOUCH, NULL) > 0 && !strcmp(clip, "https://example.com/qr"), "URL copy uses existing clipboard capability");
    scan_image("text", "text", "Hello QR!"); touch("clipboard.write");
    CHECK(ink_clip_get(clip, sizeof clip, INK_SRC_TOUCH, NULL) > 0 && strstr(clip, "Hello QR!"), "plain Chinese/English text can be copied");
    scan_image("mecard", "contact", "10000000000");
    CHECK(AI("camera.scan_apply", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "contact save asks before adding"); decision(false);
    scan_image("vcard", "contact", "10000000000");
    CHECK(AI("camera.scan_apply", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "vCard save asks before adding"); decision(true);
    CHECK(WAIT_FOR(AI("data.list", "{\"collection\":\"phone.contacts\"}") && strstr(g_result, "10000000000"), 5000), "contact is saved by contacts.put");
    scan_image("tel", "tel", "10000000000");
    CHECK(AI("camera.scan_apply", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "telephone asks before dialing"); capture("tel-zh"); decision(true);
    CHECK(WAIT_FOR(sim_net_call(NULL, 0) != 0, 10000), "telephone uses existing call capability");
    CHECK(BY_HAND("call.hangup", NULL), "hang up fake modem call");
    scan_image("wifi", "wifi", "qr-test"); capture("wifi-zh");
    CHECK(AI("camera.scan_apply", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "Wi-Fi asks before connecting"); on_ui(snap); decision(true);
    char state[24];
    CHECK(WAIT_FOR(ink_state_get_str(ink_state_find("wifi.state"), state, sizeof state) && !strcmp(state, "connected"), 10000), "Wi-Fi connect finishes");
    scan_image("escaped", "wifi", "qr;test");
    scan_image("tilted", "url", "example.com"); scan_image("blurred", "url", "example.com"); scan_image("small", "url", "example.com");
    scan_image("blank", NULL, NULL); CHECK(strstr(g_rc.say, "重试"), "unrecognized image has an explicit retry message"); capture("blank-zh");
    CHECK(BY_HAND("settings.set", "{\"id\":\"system.language\",\"value\":1}") && g_rc.status == INK_RC_DONE, "switch to English");
    scan_image("url", "url", "example.com"); capture("url-en"); on_ui(snap); CHECK(strstr(g_snap, "Scan QR code"), "English AI snapshot");
    scan_image("wifi", "wifi", "qr-test"); capture("wifi-en");
    CHECK(ink_member_switch("kid") == 0, "enter child's space with adult scan still open"); quiet();
    CHECK(!result, "member switch discards adult credentials");
    scan_image("text", "text", "Hello QR!");
    CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "leave scan");
    CHECK(AI("camera.scan_apply", NULL) && g_rc.status != INK_RC_DONE && !result, "leaving page clears private result");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(options, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "qr-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start test driver");
}
