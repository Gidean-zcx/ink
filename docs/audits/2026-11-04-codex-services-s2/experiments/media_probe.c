/* Whole product; ordinary public capability calls, no injected product changes. */
#include "product_probe.h"
static volatile int g_rec_done;
static char g_rec_path[128];
static int64_t g_rec_ms;
static void on_event(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    ink_args_get_str(a, "path", g_rec_path, sizeof g_rec_path);
    ink_args_get_int(a, "ms", &g_rec_ms);
    g_rec_done++;
}
static ink_media_status_t st(void) {
    ink_media_status_t s;
    ink_media_status(&s);
    return s;
}
static bool record(const char *path, int seconds) {
    char args[160];
    snprintf(args, sizeof args, "{\"path\":\"%s\"}", path);
    int done = g_rec_done;
    if (!BY_HAND("audio.record", args) || g_rc.status != INK_RC_DONE) return false;
    delay_ms(seconds * 1000);
    if (!BY_HAND("audio.record_stop", NULL)) return false;
    return WAIT_FOR(g_rec_done == done + 1, 3000);
}

static void audit_task(void *arg) {
    (void)arg;
    delay_ms(1800);
    ink_cap_subscribe("audio.record_done", on_event, NULL, INK_EXEC_DIRECT);
    CHECK(record("/me/rec/audit.opus", 12), "record fixture");
    CHECK(BY_HAND("media.play", "{\"path\":\"/me/rec/audit.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "play fixture");
    delay_ms(4000);
    CHECK(BY_HAND("media.pause", NULL) && g_rc.status == INK_RC_DONE, "pause");
    uint32_t paused = st().position_ms;
    CHECK(BY_HAND("media.stop", NULL) && g_rc.status == INK_RC_DONE, "stop while paused");
    CHECK(BY_HAND("media.play", "{\"path\":\"/me/rec/audit.opus\"}") && g_rc.status == INK_RC_DONE, "resume saved file");
    uint32_t resumed = st().position_ms;
    printf("AUDIT media paused_ms=%u reopened_ms=%u\n", paused, resumed);
    CHECK(paused > 3000 && resumed < 1000, "reproduce lost paused position");
    BY_HAND("media.stop", NULL);
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/rec/switch.opus\"}") && g_rc.status == INK_RC_DONE, "record as owner");
    delay_ms(1800);
    CHECK(ink_member_switch("kid") == 0, "switch to child");
    delay_ms(2200);
    printf("AUDIT recording after member switch member=%s recording=%d\n", ink_member_current()->id, ink_audio_recording());
    CHECK(ink_audio_recording(), "reproduce owner recording continuing in child's session");
    int before = g_rec_done;
    BY_HAND("audio.record_stop", NULL);
    CHECK(WAIT_FOR(g_rec_done > before, 3000), "record finished");
    ink_fs_stat_t fs;
    int child_file = ink_fs_stat("/me/rec/switch.opus", &fs);
    printf("AUDIT record_done current=%s path=%s ms=%lld child_stat=%d\n", ink_member_current()->id, g_rec_path, (long long)g_rec_ms, child_file);
    CHECK(g_rec_ms > 3000 && child_file != 0, "record includes child interval but belongs to owner");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-media", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c, NULL);
}
