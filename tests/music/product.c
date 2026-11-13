#include <stdlib.h>
#include "ink/ui_shell.h"
#include "ink/sys/media.h"
#include "ink/sys/audio.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "../../apps/music/port/music_page.h"
static char snapshot[12000];
static const char *stack_format;
static void read_format(void *arg) { (void)arg; stack_format = getenv("INK_MUSIC_STACK_FORMAT"); }
static ink_media_status_t status(void) { ink_media_status_t s; ink_media_status(&s); return s; }
static void sample(void) { CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "music snapshot"); }
static bool has(const char *text) { on_ui(sample); return strstr(snapshot, text) != NULL; }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static void ok(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s: %s %.120s", cap, ink_rc_name(g_rc.status), g_rc.detail);
}
static void pause_at(unsigned ms) {
    ok("music.pause", NULL); CHECK(status().state == INK_MEDIA_PAUSED, "pause completes before receipt");
    char args[48]; snprintf(args, sizeof args, "{\"ms\":%u}", ms); ok("music.seek", args); CHECK(status().position_ms == ms, "seek applies while paused");
}
static void capture(void *arg) { sim_display_snapshot(arg); }
static void evidence(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(5); }
/* The page draws on the graft surface: its tap targets come from the page's own hit table, read on its executor. */
static EpdRect targets[48];
static int ntargets, x, y;
static volatile bool listed;
static ink_exec_job_t list_job;
static void read_targets(void *arg) { (void)arg; ntargets = music_page_targets(targets, 48); listed = true; }
static void locate(void) {
    quiet(); listed = false; ink_exec_post(INK_EXEC_BG, &list_job, read_targets, NULL); CHECK(WAIT_FOR(listed, 3000), "page hit table");
    const int top = ink_ui_px(INK_STATUS_BAR), min = ink_ui_px(INK_CONTROL);
    for (int i = 0; i < ntargets; i++) {
        const EpdRect r = targets[i];
        CHECK(r.width >= min && r.height >= min, "music target at least 108 px (%dx%d)", r.width, r.height);
        CHECK(r.x >= 0 && r.y >= 0 && r.x + r.width <= ink_ui_width() && top + r.y + r.height <= ink_ui_height(), "music control fits %d,%d", r.x, r.y);
        for (int k = 0; k < i; k++)
            CHECK(r.x >= targets[k].x + targets[k].width || targets[k].x >= r.x + r.width || r.y >= targets[k].y + targets[k].height || targets[k].y >= r.y + r.height,
                  "music rows clear the directory and paging controls");
    }
}
/* The nth full-width target: the progress bar first, then the list rows. */
static EpdRect wide(int nth) {
    int widest = 0;
    for (int i = 0; i < ntargets; i++) if (targets[i].width > widest) widest = targets[i].width;
    for (int i = 0; i < ntargets; i++) if (targets[i].width == widest && !nth--) return targets[i];
    return (EpdRect){-1, -1, 0, 0};
}
static void touch_at(int px, int py) {
    x = px; y = ink_ui_px(INK_STATUS_BAR) + py;
    double before = sim_metric_value("input.touch_delivery_ms"); tap(x, y);
    CHECK(WAIT_FOR(sim_metric_value("input.touch_delivery_ms") > before, 1000), "touch delivered"); quiet();
}
/* Tap the progress bar where `ms` falls; the page seeks to what that point means for the song now playing. */
static unsigned seek_by_touch(unsigned ms) {
    locate(); EpdRect bar = wide(0); unsigned duration = status().duration_ms;
    CHECK(bar.width > 0 && duration > 0, "touch binding music.seek");
    unsigned rel = (unsigned)(((uint64_t)ms * (unsigned)bar.width + duration - 1) / duration);
    touch_at(bar.x + (int)rel, bar.y + bar.height / 2);
    return (unsigned)((uint64_t)duration * rel / (unsigned)bar.width);
}
/* The bookmark column of the playing song's row. */
static void favorite_by_touch(void) {
    ok("music.list", "{\"view\":\"player\"}"); const char *at = strstr(g_result, status().path); int row = 0;
    for (const char *p = strstr(g_result, "\"items\":["); p && at && (p = strstr(p, "{\"path\":")) && p < at; p++) row++;
    row--;
    locate(); EpdRect r = wide(1 + row);
    CHECK(at && r.width > 0, "touch binding music.favorite");
    touch_at(r.x + ink_ui_px(INK_CONTROL) / 2, r.y + r.height / 2);
}
static void switch_to(const char *id) {
    char args[48]; snprintf(args, sizeof args, "{\"id\":\"%s\"}", id);
    CHECK(BY_HAND("system.switch_member", args) && g_rc.status == INK_RC_DONE, "switch %.20s", id);
    CHECK(WAIT_FOR(ink_member_space_open(), 5000), "member opens");
}
static void test(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "boot opens member"); test_start_home();
    ok("app.open", "{\"id\":\"music\"}");
    if (stack_format) {
        char args[180]; snprintf(args, sizeof args, "{\"path\":\"/family/audio/tone-44100-2.%s\"}", stack_format);
        ok("music.play", args); pause_at(1000); ok("music.play", NULL); ink_media_stop();
        CHECK(WAIT_FOR(status().state == INK_MEDIA_IDLE, 3000), "format stream closes");
        sim_osal_report();
        double used = sim_metric_value("task.exec.media.stack_used_est_bytes");
        CHECK(used < 8192, "media stack estimate %.0f < 8192", used);
        sim_note("music stack %s: %.0f bytes, %.0f bytes spare", stack_format, used, 8192 - used);
        ink_probe_count("test.finished"); return;
    }
    ok("music.list", "{\"path\":\"/family/audio\",\"view\":\"all\"}"); CHECK(strstr(g_result, "items") && strstr(g_result, "合成音调"), "AI lists tagged songs: %.150s", g_result);
    CHECK(WAIT_FOR(has("合成音调"), 3000), "list reaches the page");
    ok("music.play", "{\"path\":\"/family/audio/文件夹/快进测试.wav\"}"); pause_at(1000);
    ok("music.list", "{\"view\":\"player\"}"); CHECK(WAIT_FOR(has("0:01"), 3000), "skip player starts at one second");
    pause_at(2000); CHECK(WAIT_FOR(has("0:02"), 3000), "partial update reaches two seconds");
    unsigned expect = seek_by_touch(12000); CHECK(WAIT_FOR(status().position_ms == expect, 2000), "skip uses updated progress");
    pause_at(3000); CHECK(WAIT_FOR(has("0:03"), 3000), "partial update reaches three seconds");
    expect = seek_by_touch(13000); CHECK(WAIT_FOR(status().position_ms == expect, 2000), "skip updates again without rebuilding");
    const char *const files[] = {"tone-16000-1.mp3", "tone-16000-1.flac", "tone-16000-1.wav", "tone-16000-1.ogg"};
    for (unsigned i = 0; i < sizeof files / sizeof *files; i++) {
        char args[180]; snprintf(args, sizeof args, "{\"path\":\"/family/audio/%s\"}", files[i]); ok("music.play", args);
        CHECK(status().state == INK_MEDIA_PLAYING && strstr(status().path, files[i]), "format plays and switches %.40s", files[i]); pause_at(1000);
        ok("music.now", NULL); CHECK(strstr(g_result, "\"position_ms\":1000"), "AI sees seek: %.150s", g_result);
    }
    CHECK(AI("music.play", "{\"path\":\"/family/audio/bad.mp3\"}") && g_rc.status != INK_RC_DONE, "bad file rejected");
    ok("music.play", "{\"path\":\"/family/audio/tone-16000-1.flac\"}"); pause_at(1000);
    char original[128]; snprintf(original, sizeof original, "%s", status().path); ok("music.next", NULL); CHECK(strcmp(status().path, original), "next changes track");
    ok("music.prev", NULL); CHECK(!strcmp(status().path, original), "prev returns to previous track"); pause_at(1000);
    ok("music.list", "{\"view\":\"player\"}"); CHECK(WAIT_FOR(has("0:01"), 3000), "page shares progress");
    favorite_by_touch(); ok("music.now", NULL); CHECK(strstr(g_result, "\"favorite\":true"), "touch favorites the playing song");
    favorite_by_touch(); ok("music.now", NULL); CHECK(strstr(g_result, "\"favorite\":false"), "touch unfavorites it");
    for (int scale = 1; scale <= 3; scale += 2) {
        char args[96]; snprintf(args, sizeof args, "{\"id\":\"display.font_size\",\"value\":%d}", scale); ok("settings.set", args);
        ok("music.list", "{\"view\":\"player\"}"); CHECK(WAIT_FOR(has("0:01"), 3000), "player snapshot ready");
        evidence(scale == 1 ? "player-100" : "player-120"); locate();
        ok("music.list", "{\"view\":\"list\",\"page\":0}"); CHECK(WAIT_FOR(has("track0"), 3000), "playlist snapshot ready"); evidence(scale == 1 ? "list-100" : "list-120"); locate();
        ok("music.list", "{\"view\":\"all\",\"page\":0}"); quiet(); evidence(scale == 1 ? "all-100" : "all-120"); locate();
    }
    ok("music.list", "{\"page\":1}"); CHECK(WAIT_FOR(has("track0"), 3000), "next list page");
    switch_to("kid"); ok("app.open", "{\"id\":\"music\"}"); ok("music.play", "{\"path\":\"/family/audio/tone-16000-1.flac\"}"); pause_at(2000);
    CHECK(AI("music.play", "{\"path\":\"/members/owner/private.mp3\"}") && g_rc.status != INK_RC_DONE, "child cannot play adult files");
    switch_to("owner"); ok("app.open", "{\"id\":\"music\"}"); ok("music.play", "{\"path\":\"/family/audio/tone-16000-1.flac\"}");
    CHECK(status().position_ms >= 1000 && status().position_ms < 2000, "adult resumes own position (%u)", (unsigned)status().position_ms);
    int focus = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(focus > 0 && WAIT_FOR(status().state == INK_MEDIA_PAUSED, 2000), "call focus pauses new format"); ink_audio_focus_release(focus);
    CHECK(WAIT_FOR(status().state == INK_MEDIA_PLAYING, 2000), "focus release resumes new format");
    ink_media_stop(); CHECK(WAIT_FOR(status().state == INK_MEDIA_IDLE, 3000), "stop releases stream"); ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(read_format, NULL);
    product_main(); osal_task_cfg_t c = {.name = "musictest", .fn = test, .stack_bytes = 24576, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&c, NULL) != OSAL_OK) osal_panic("music test task");
}
