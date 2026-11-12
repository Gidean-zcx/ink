/* Real service/output model, synthetic member-scoped media only. */
#include "../services_finish/product.h"
#include "ink/hal_codec.h"
static char mode[24];
static void read_mode(void *arg) { (void)arg; sim_copy_name(mode, sizeof mode, getenv("INK_AUDIO_OUT_MODE")); }
static ink_media_status_t status(void) { ink_media_status_t s; ink_media_status(&s); return s; }
static void ok(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%.40s: %.100s", cap, g_rc.detail);
}
static void redraw(void) { ink_ui_redraw(); }
static void play(const char *path) {
    char args[200]; snprintf(args, sizeof args, "{\"path\":\"%s\",\"start_ms\":0}", path);
    ok("media.play", args);
    CHECK(WAIT_FOR(status().state == INK_MEDIA_PLAYING, 4000), "playing");
}
static void until(unsigned ms) {
    CHECK(WAIT_FOR(status().position_ms >= ms, 10000), "heard %u ms (at %u)", ms, status().position_ms);
}
static void stopped(void) {
    ink_media_stop();
    CHECK(WAIT_FOR(status().state == INK_MEDIA_IDLE, 3000), "stopped");
    delay_ms(100);
}
static void test(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member opens");
    test_start_home();
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "home settles");
    ink_audio_set_volume(100);
    if (strcmp(mode, "scenario")) {
        char path[100]; snprintf(path, sizeof path, "/family/audio/%s.wav", mode);
        play(path);
        until(3000);
        stopped();
    } else {
        play("/family/audio/01.flac");
        until(1200);
        on_ui(redraw);
        CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "refresh during FLAC");
        ink_media_pause();
        CHECK(WAIT_FOR(status().state == INK_MEDIA_PAUSED, 3000), "pause");
        delay_ms(100);
        double silent = sim_audio_played_ms();
        delay_ms(300);
        CHECK(sim_audio_played_ms() == silent, "paused output stops");
        CHECK(ink_media_seek(60000) == 0, "seek accepted");
        CHECK(WAIT_FOR(status().position_ms == 60000, 3000), "seek to one minute");
        ink_media_resume();
        CHECK(WAIT_FOR(status().state == INK_MEDIA_PLAYING, 3000), "resume");
        until(61200);
        /* Next file through the same media executor used by music.next. */
        play("/family/audio/02.mp3");
        until(1200);
        int focus = ink_audio_focus_acquire(INK_AUDIO_PRIO_VOICE, NULL, NULL, INK_EXEC_DIRECT);
        CHECK(focus > 0 && WAIT_FOR(status().state == INK_MEDIA_PAUSED, 3000), "speech preempts music");
        int voice = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
        CHECK(voice > 0, "speech stream opens");
        uint8_t packet[180];
        int n = sim_opus_fake_packet(packet, sizeof packet, 2200, 6000, sizeof packet, 1);
        for (int i = 0; i < 15; i++) CHECK(ink_audio_stream_push(voice, packet, (size_t)n) == 0, "speech frame");
        ink_audio_stream_finish(voice);
        CHECK(WAIT_FOR(ink_audio_stream_progress(voice, NULL, NULL) < 0, 5000), "speech finishes");
        ink_audio_focus_release(focus);
        CHECK(WAIT_FOR(status().state == INK_MEDIA_PLAYING, 3000), "music resumes after speech");
        until(1200);
        on_ui(redraw);
        CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "refresh during MP3");
        CHECK(BY_HAND("system.switch_member", "{\"id\":\"kid\"}") && g_rc.status == INK_RC_DONE, "switch member");
        CHECK(WAIT_FOR(status().state == INK_MEDIA_IDLE, 4000), "member stops old music");
        delay_ms(100);
        silent = sim_audio_played_ms();
        delay_ms(300);
        CHECK(sim_audio_played_ms() == silent, "old member output stops");
        play("/family/audio/03.wav");
        until(1200);
        stopped();
    }
    uint64_t underruns; double gap, idle;
    sim_audio_speaker_counters(&underruns, &gap, &idle);
    CHECK(underruns == 0, "no underruns: %llu, %.2f ms", (unsigned long long)underruns, gap);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(read_mode, NULL);
    product_main();
    osal_task_cfg_t cfg = {.name = "audio-out-test", .fn = test, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
