#include "calls.h"
#define ink_cap_call fx_call
#ifndef INK_UI_FIX_SOURCE
#define INK_UI_FIX_SOURCE "../../system/ui/shell/transfer_pages.c"
#endif
#include INK_UI_FIX_SOURCE
#undef ink_cap_call
#include "common.h"
static const char *fx_result;
static lv_area_t fx_close;
static void fx_load(void) {
    ink_receipt_t rc = {.status = INK_RC_DONE, .result = fx_result};
    P.pending = false; reply((void *)(uintptr_t)P.generation, &rc);
    if (P.timer) lv_timer_pause(P.timer);
}
static void fx_open(void) { ink_call_t c = {.cap = ink_cap_find("transfer.open")}; open_page(&c); }
static void fx_close_position(void) {
    fx_wanted = "关闭"; fx_find();
    CHECK(fx_label && !memcmp(&fx_area, &fx_close, sizeof fx_area), "UX11 close stays at the same position and size");
}
static void fx_usb_busy(void) {
    P.usb = true; P.pending = false;
    ink_receipt_t rc = {.status = INK_RC_FAILED};
    snprintf(rc.detail, sizeof rc.detail, "busy"); reply((void *)(uintptr_t)P.generation, &rc);
    if (P.timer) lv_timer_pause(P.timer);
    P.pending = false;
}
static void fx_rows(void) {
    fx_result = "{\"active\":true,\"host\":\"example.com\",\"port\":8080,\"code\":\"000000\",\"files\":0,\"received\":0,\"result\":\"等待上传\"}";
    fx_load();
    lv_obj_t *retained = lv_obj_get_child(P.screen, 0);
    lv_obj_add_flag(retained, LV_OBJ_FLAG_USER_4);
    CHECK(!P.file_rows[0] && !P.file_rows[1], "no file list is drawn before receipt");
    fx_result = "{\"active\":true,\"host\":\"example.com\",\"port\":8080,\"code\":\"000000\",\"files\":1,\"last_file\":\"第一本.txt\",\"received\":100,\"result\":\"文件已收到\"}";
    fx_load();
    CHECK(lv_obj_get_child(P.screen, 0) == retained && lv_obj_has_flag(retained, LV_OBJ_FLAG_USER_4) && P.file_rows[0], "first receipt retains the page and adds one row");
    lv_obj_t *first = P.file_rows[0];
    fx_result = "{\"active\":true,\"host\":\"example.com\",\"port\":8080,\"code\":\"000000\",\"files\":2,\"last_file\":\"第二本.txt\",\"received\":200,\"result\":\"文件已收到\"}";
    fx_load();
    CHECK(lv_obj_get_child(P.screen, 0) == retained && lv_obj_has_flag(retained, LV_OBJ_FLAG_USER_4) && P.file_rows[0] == first && P.file_rows[1], "second receipt preserves the first row and adds only the new row");
    CHECK(strstr(P.file_text, "第一本.txt") && strstr(P.file_text, "第二本.txt"), "AI receives both committed filenames");
}
static void run(void *arg) {
    (void)arg; fx_start(); on_ui(fx_open); fx_capture("idle");
    fx_wanted = "关闭"; on_ui(fx_find); fx_close = fx_area;
    fx_result = "{\"active\":true,\"host\":\"example.com\",\"port\":8080,\"code\":\"654321\",\"received\":0,\"result\":\"等待上传\"}";
    on_ui(fx_load); fx_capture("waiting"); on_ui(fx_close_position);
    CHECK(!strcmp(P.progress, "等待传入"), "UX11 an idle authorization does not claim a file is being received");
    CHECK(!strstr(g_snap, "654321"), "UX11 one-time code is masked from AI");
    fx_result = "{\"active\":true,\"host\":\"example.com\",\"port\":8080,\"code\":\"654321\",\"received\":4096,\"result\":\"等待上传\"}";
    on_ui(fx_load); fx_capture("receiving"); on_ui(fx_close_position);
    CHECK(strstr(g_snap, "尚未保存") && strstr(g_snap, "现在还不能打开") && !strstr(g_snap, "已收到 0 个"), "UX11 partial bytes are not a received or openable file");
    fx_result = "{\"active\":false,\"received\":12288,\"result\":\"文件已收到\"}";
    on_ui(fx_load); fx_capture("received"); on_ui(fx_close_position);
    CHECK(strstr(g_snap, "已收到 1 个，12288 字节") && strstr(g_snap, "可在文件中打开"), "UX11 committed receipt says where the file can open");
    on_ui(fx_usb_busy); fx_capture("usb-busy");
    CHECK(strstr(g_snap, "不能自动停下") && !strstr(g_snap, "停下，开始"), "UX11 USB busy does not promise to stop applications");
    fx_press("返回先停下"); CHECK(!strcmp(fx_called, "shell.back"), "UX11 busy exit returns without mounting USB");
    on_ui(fx_open); on_ui(fx_rows); fx_capture("received-native");
    ink_probe_count("test.finished");
}
void ink_main(void) { product_main(); fx_task(run); }
