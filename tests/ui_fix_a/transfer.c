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
}
static void run(void *arg) {
    (void)arg; fx_start(); on_ui(fx_open); fx_capture("idle");
    fx_wanted = "关闭"; on_ui(fx_find); fx_close = fx_area;
    fx_result = "{\"active\":true,\"host\":\"example.com\",\"port\":8080,\"code\":\"654321\",\"received\":0,\"result\":\"等待上传\"}";
    on_ui(fx_load); fx_capture("waiting"); on_ui(fx_close_position);
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
    ink_probe_count("test.finished");
}
void ink_main(void) { product_main(); fx_task(run); }
