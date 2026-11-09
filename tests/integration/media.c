/*
 * Audio focus, media playback, recording to a file and the headset (system services design §10), on the whole product:
 * files are recorded from the simulated microphone, played back through the simulated speaker, cut, damaged and pulled
 * away; a call and an alarm come in between. Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_storage.h"
#include "ink/sys/ai.h"
#include "ink/sys/audio.h"
#include "ink/sys/confirm.h"
#include "ink/sys/file.h"
#include "ink/sys/media.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/hub.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

#define HEADSET "00:11:22:33:44:55"

static volatile int g_finished, g_rec_done;
static char g_fin_why[24], g_rec_why[24], g_rec_path[128];
static int64_t g_rec_ms;
static void on_event(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    if (!strcmp(id, "media.finished")) {
        ink_args_get_str(a, "why", g_fin_why, sizeof g_fin_why);
        g_finished++;
    } else if (!strcmp(id, "audio.record_done")) {
        ink_args_get_str(a, "why", g_rec_why, sizeof g_rec_why);
        ink_args_get_str(a, "path", g_rec_path, sizeof g_rec_path);
        ink_args_get_int(a, "ms", &g_rec_ms);
        g_rec_done++;
    }
}

static const char *state_of(const char *id) {
    static char s[32];
    s[0] = 0;
    ink_state_get_str(ink_state_find(id), s, sizeof s);
    return s;
}
static bool bool_state(const char *id) {
    int64_t v = 0;
    ink_state_get_int(ink_state_find(id), &v);
    return v != 0;
}
static ink_media_status_t st(void) {
    ink_media_status_t s;
    ink_media_status(&s);
    return s;
}

/* A recording of this many seconds; the file's name. */
static bool record(const char *path, int seconds) {
    char args[160];
    snprintf(args, sizeof args, "{\"path\":\"%s\"}", path);
    int done = g_rec_done;
    if (!BY_HAND("audio.record", args) || g_rc.status != INK_RC_DONE) return false;
    delay_ms(seconds * 1000);
    if (!BY_HAND("audio.record_stop", NULL)) return false;
    return WAIT_FOR(g_rec_done == done + 1, 3000);
}

static ink_exec_job_t g_key_job0;
static void key_job0(void *pressed) { ink_ai_key(pressed != NULL, osal_now_us()); }
static void dismiss_sheet(void) { ink_ai_dismiss(); }

static volatile int g_pin_done, g_pin_result;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx;
    (void)wait_s;
    g_pin_result = result;
    g_pin_done++;
}

static void media_busy(void *arg) { osal_task_delay_us((uint64_t)(intptr_t)arg * 1000u); }

static void recording(void) {
    int done = g_rec_done;
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/rec/one.opus\"}") && g_rc.status == INK_RC_DONE && strstr(g_rc.object, "one.opus"), "recording starts (%s %s)", ink_rc_name(g_rc.status), g_rc.detail);
    CHECK(strstr(g_result, "/me/rec/one.opus"), "the result says where (%s)", g_result);
    CHECK(bool_state("audio.recording") && ink_audio_recording(), "state says so");
    CHECK(WAIT_FOR(!strcmp(state_of("audio.mic"), "recording"), 200), "the microphone says recording (%s)", state_of("audio.mic"));
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/rec/two.opus\"}") && g_rc.status == INK_RC_FAILED, "one at a time");
    CHECK(ink_audio_capture_begin(NULL, NULL) != 0, "the microphone is taken");

    /* the AI key meanwhile: says why not */
    double mic0 = sim_metric_value("ai.mic_unavailable");
    ink_exec_post(INK_EXEC_UI, &g_key_job0, key_job0, (void *)1);
    delay_ms(1000);
    ink_exec_post(INK_EXEC_UI, &g_key_job0, key_job0, NULL);
    CHECK(sim_metric_value("ai.mic_unavailable") == mic0 + 1, "the AI cannot have the microphone now");
    on_ui(dismiss_sheet);
    delay_ms(5000);
    CHECK(BY_HAND("audio.record_stop", NULL) && g_rc.status == INK_RC_DONE, "stop");
    CHECK(WAIT_FOR(g_rec_done == done + 1, 3000), "it ends and says so");
    CHECK(!strcmp(g_rec_why, "stopped") && strstr(g_rec_path, "one.opus") && g_rec_ms >= 5500 && g_rec_ms <= 7000, "with its length (%s, %lld)", g_rec_why, (long long)g_rec_ms);
    CHECK(!bool_state("audio.recording"), "state again");
    CHECK(WAIT_FOR(!strcmp(state_of("audio.mic"), "off"), 200), "the microphone is off (%s)", state_of("audio.mic"));
    ink_fs_stat_t fs;
    CHECK(ink_fs_stat("/me/rec/one.opus", &fs) == 0 && fs.size > 1000, "the file is there (%lld)", (long long)fs.size);

    /* someone else asking */
    CHECK(AI("audio.record", "{\"max_seconds\":5}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "the AI turning the microphone on needs a yes (%s)", ink_rc_name(g_rc.status));
    unsigned token = 0;
    sscanf(g_rc.object, "confirm:%u", &token);
    CHECK(ink_confirm_withdraw(token, INK_SRC_AI) == 0, "taken back");
    delay_ms(1200);

    /* no name given; it stops by itself */
    done = g_rec_done;
    CHECK(BY_HAND("audio.record", "{\"max_seconds\":2}") && g_rc.status == INK_RC_DONE && strstr(g_result, "/me/recordings/2026"), "a name of its own (%s)", g_result);
    CHECK(WAIT_FOR(g_rec_done == done + 1, 5000) && !strcmp(g_rec_why, "max_length") && g_rec_ms >= 2000 && g_rec_ms < 3000, "as long as asked (%s, %lld)", g_rec_why, (long long)g_rec_ms);

    /* a call: what there is, is kept */
    done = g_rec_done;
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/rec/cut.opus\"}") && g_rc.status == INK_RC_DONE, "again");
    delay_ms(2000);
    int f = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(WAIT_FOR(g_rec_done == done + 1, 3000) && !strcmp(g_rec_why, "interrupted"), "a call ends it (%s)", g_rec_why);
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/rec/nope.opus\"}") && g_rc.status == INK_RC_FAILED, "and none starts during one");
    ink_audio_focus_release(f);
    CHECK(ink_fs_stat("/me/rec/cut.opus", &fs) == 0 && fs.size > 500, "what there was is on the card");

    /* someone else takes the device (AUD16-003): it ends, what there is stays with the one who recorded it, the next one's voice is not in it */
    done = g_rec_done;
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/rec/whose.opus\"}") && g_rc.status == INK_RC_DONE, "recording, as the owner");
    delay_ms(1800);
    static ink_exec_job_t busy; /* the media executor has something else to do first: the microphone goes on meanwhile */
    ink_exec_post(INK_EXEC_MEDIA, &busy, media_busy, (void *)(intptr_t)1000);
    CHECK(ink_member_switch("kid") == 0, "someone else");
    CHECK(WAIT_FOR(g_rec_done == done + 1, 2000) && !strcmp(g_rec_why, "member_changed"), "it ends (%s)", g_rec_why);
    CHECK(!ink_audio_recording() && !bool_state("audio.recording"), "and says so");
    CHECK(g_rec_ms >= 1500 && g_rec_ms <= 2100, "what was said before, no more (%d ms)", (int)g_rec_ms);
    CHECK(ink_fs_stat("/me/rec/whose.opus", &fs) == INK_FS_ERR_NOT_FOUND, "not in the new one's files");
    ink_member_test_reset();
    delay_ms(300);
    CHECK(ink_fs_stat("/me/rec/whose.opus", &fs) == 0 && fs.size > 500, "it is the owner's, and it can be played");
    CHECK(BY_HAND("media.play", "{\"path\":\"/me/rec/whose.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "a whole file");
    BY_HAND("media.stop", NULL);

    /* the card is slow for a long while (AUD17-005): what does not fit in the queue is lost, its end is not */
    done = g_rec_done;
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/rec/slow.opus\"}") && g_rc.status == INK_RC_DONE, "recording");
    delay_ms(400);
    ink_exec_post(INK_EXEC_MEDIA, &busy, media_busy, (void *)(intptr_t)9000);
    delay_ms(8500);
    CHECK(BY_HAND("audio.record_stop", NULL) && g_rc.status == INK_RC_DONE, "stopped, with the queue full");
    CHECK(WAIT_FOR(g_rec_done == done + 1, 3000) && !strcmp(g_rec_why, "stopped"), "it ends all the same (%s)", g_rec_why);
    CHECK(!ink_audio_recording() && sim_metric_value("media.record_lost_frames") > 0, "and frames were lost, as it says (%.0f)", sim_metric_value("media.record_lost_frames"));
    CHECK(ink_fs_stat("/me/rec/slow.opus", &fs) == 0 && fs.size > 500, "what fitted is there");

    /* the card fills up */
    done = g_rec_done;
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/rec/full.opus\"}") && g_rc.status == INK_RC_DONE, "and again");
    delay_ms(1500);
    sim_sd_set_free(0);
    CHECK(WAIT_FOR(g_rec_done == done + 1, 8000) && !strcmp(g_rec_why, "no_space"), "full: it stops and says why (%s)", g_rec_why);
    sim_sd_set_free(-1);
}

static void playing(void) {
    CHECK(record("/me/rec/long.opus", 40), "a longer file");
    CHECK(AI("media.play", "{\"path\":\"/me/rec/nothing.opus\"}") && g_rc.status == INK_RC_FAILED, "no such file");
    ink_fs_write_atomic("/me/rec/not.opus", "this is not audio at all, not even close to it", 46);
    CHECK(AI("media.play", "{\"path\":\"/me/rec/not.opus\"}") && g_rc.status == INK_RC_FAILED && strstr(g_rc.detail, "Ogg"), "not a file it can play (%s)", g_rc.detail);
    CHECK(!strcmp(state_of("media.state"), "idle"), "nothing plays");

    double heard0 = sim_audio_played_ms();
    CHECK(AI("media.play", "{\"path\":\"/me/rec/long.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "the AI starts it (%s)", g_rc.detail);
    CHECK(!strcmp(state_of("media.state"), "playing"), "playing");
    delay_ms(3000);
    double heard = sim_audio_played_ms() - heard0;
    CHECK(heard > 2000 && heard < 3300, "it is heard (%.0f ms)", heard);
    ink_media_status_t s = st();
    CHECK(s.duration_ms >= 39000 && s.duration_ms <= 41000 && !strcmp(s.path, "/me/rec/long.opus"), "it knows how long (%u)", (unsigned)s.duration_ms);
    CHECK(AI("media.status", NULL) && strstr(g_result, "\"state\":\"playing\"") && strstr(g_result, "\"duration_ms\":"), "the AI can ask (%s)", g_result);
    /* pause */
    CHECK(BY_HAND("media.pause", NULL) && g_rc.status == INK_RC_DONE, "pause");
    s = st();
    CHECK(s.state == INK_MEDIA_PAUSED && s.paused_by == INK_MEDIA_BY_USER && s.position_ms > 2000 && s.position_ms < 3600, "where it stopped (%u)", (unsigned)s.position_ms);
    uint32_t at = s.position_ms;
    delay_ms(400);
    heard0 = sim_audio_played_ms();
    delay_ms(1000);
    CHECK(sim_audio_played_ms() <= heard0 + 200, "silence (%.0f)", sim_audio_played_ms() - heard0);
    /* resume */
    CHECK(BY_HAND("media.resume", NULL) && g_rc.status == INK_RC_DONE, "resume");
    delay_ms(1000);
    s = st();
    CHECK(s.state == INK_MEDIA_PLAYING && s.position_ms >= at && s.position_ms < at + 1500, "on from there (%u, %u)", (unsigned)at, (unsigned)s.position_ms);
    /* seek */
    CHECK(BY_HAND("media.seek", "{\"ms\":20000}") && g_rc.status == INK_RC_DONE, "seek");
    s = st();
    CHECK(s.position_ms > 18500 && s.position_ms <= 20000, "to about there (%u)", (unsigned)s.position_ms);
    delay_ms(1500);
    s = st();
    CHECK(s.position_ms > 19000 && s.position_ms < 22000 && s.state == INK_MEDIA_PLAYING, "and on (%u)", (unsigned)s.position_ms);

    /* background: the page goes, the device locks: it goes on */
    heard0 = sim_audio_played_ms();
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "locked");
    delay_ms(4000);
    CHECK(sim_audio_played_ms() > heard0 + 2500 && !strcmp(state_of("media.state"), "playing"), "it goes on, locked (%.0f)", sim_audio_played_ms() - heard0);
    ink_power_unlock(osal_now_us());
    delay_ms(300);

    /* to the end */
    int fin = g_finished;
    CHECK(BY_HAND("media.seek", "{\"ms\":37000}"), "near the end");
    CHECK(WAIT_FOR(g_finished == fin + 1, 8000) && !strcmp(g_fin_why, "finished"), "it ends and says so (%s)", g_fin_why);
    CHECK(!strcmp(state_of("media.state"), "idle"), "idle");
    /* from the start next time */
    CHECK(AI("media.play", "{\"path\":\"/me/rec/long.opus\"}") && g_rc.status == INK_RC_DONE, "again, no place given");
    s = st();
    CHECK(s.position_ms < 500, "heard to the end: from the start (%u)", (unsigned)s.position_ms);
    /* where it was: stopped halfway, it comes back */
    CHECK(BY_HAND("media.seek", "{\"ms\":12000}"), "halfway");
    delay_ms(1000);
    CHECK(BY_HAND("media.stop", NULL) && g_rc.status == INK_RC_DONE, "stop");
    CHECK(!strcmp(state_of("media.state"), "idle"), "stopped");
    CHECK(AI("media.play", "{\"path\":\"/me/rec/long.opus\"}") && g_rc.status == INK_RC_DONE, "and again");
    s = st();
    CHECK(s.position_ms > 11000 && s.position_ms < 14000, "from where it was (%u)", (unsigned)s.position_ms);
    /* every half minute the card knows, without a flash write */
    double cached = sim_metric_value("media.positions_cached"), saved = sim_metric_value("media.positions_saved");
    CHECK(BY_HAND("media.seek", "{\"ms\":0}"), "from the start");
    saved = sim_metric_value("media.positions_saved");
    delay_ms(32000);
    CHECK(sim_metric_value("media.positions_cached") > cached, "the card has the place");
    CHECK(sim_metric_value("media.positions_saved") == saved, "the flash was not written while it played");
    ink_fs_file_t *pf;
    char pbuf[256] = "";
    int64_t pn = -1;
    CHECK(ink_fs_open("/cache/sys/media-pos", INK_FS_READ, &pf) == 0, "the place on the card");
    if (pn == -1 && ink_fs_stat("/cache/sys/media-pos", &(ink_fs_stat_t){0}) == 0) {
        pn = ink_fs_read(pf, pbuf, sizeof pbuf - 1);
        ink_fs_close(pf);
    }
    CHECK(pn > 0 && strstr(pbuf, "/me/rec/long.opus"), "of this file (%s)", pbuf);
    /* paused, then stopped (AUD16-002): the place is where the pause was, not where that stream began */
    CHECK(BY_HAND("media.play", "{\"path\":\"/me/rec/long.opus\",\"start_ms\":20000}") && g_rc.status == INK_RC_DONE, "from 20 s");
    delay_ms(4000);
    CHECK(BY_HAND("media.pause", NULL) && g_rc.status == INK_RC_DONE, "paused");
    uint32_t paused_at = st().position_ms;
    CHECK(paused_at >= 23000 && paused_at <= 25000, "at about 24 s (%u)", (unsigned)paused_at);
    CHECK(BY_HAND("media.stop", NULL) && g_rc.status == INK_RC_DONE, "stopped");
    CHECK(BY_HAND("media.play", "{\"path\":\"/me/rec/long.opus\"}") && g_rc.status == INK_RC_DONE, "and opened again");
    CHECK(st().position_ms + 3500 >= paused_at && st().position_ms <= paused_at, "from there, or the little before it that it goes back (%u, %u)", (unsigned)st().position_ms, (unsigned)paused_at);
    /* someone else: it stops, and has its own places. What was playing is not told to their AI (AUD17-004) */
    fin = g_finished;
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    int events = sim_hub_events_seen();
    CHECK(ink_member_switch("kid") == 0, "someone else");
    CHECK(WAIT_FOR(g_finished == fin + 1, 1000) && !strcmp(state_of("media.state"), "idle"), "the listening ends with the listener");
    delay_ms(500);
    for (int i = events; i < sim_hub_events_seen(); i++) CHECK(sim_hub_event_at(i) && !strstr(sim_hub_event_at(i), "long.opus"), "not told to their AI (%s)", sim_hub_event_at(i));
    ink_member_test_reset();
    delay_ms(300);
    events = sim_hub_events_seen();
    CHECK(BY_HAND("media.play", "{\"path\":\"/me/rec/long.opus\"}") && g_rc.status == INK_RC_DONE, "the owner again");
    CHECK(BY_HAND("media.stop", NULL) && g_rc.status == INK_RC_DONE, "stopped");
    bool told = false;
    for (int t = 0; t < 40 && !told; t++) {
        for (int i = events; i < sim_hub_events_seen() && !told; i++) told = sim_hub_event_at(i) && strstr(sim_hub_event_at(i), "long.opus");
        if (!told) delay_ms(50);
    }
    CHECK(told, "their own AI does hear of it");
}

/* A file bent out of shape, through the card itself. */
static int64_t copy_with(const char *from, const char *to, int64_t cut_at, int64_t flip_at) {
    hal_file_t *in = hal_fs_open(from, HAL_O_READ), *out = hal_fs_open(to, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    uint8_t *buf = malloc(4096);
    int64_t total = 0;
    while (in && out) {
        ssize_t n = hal_fs_read(in, buf, 4096);
        if (n <= 0) break;
        if (cut_at >= 0 && total + n > cut_at) n = (ssize_t)(cut_at - total);
        if (flip_at >= total && flip_at < total + n) buf[flip_at - total] ^= 0xff;
        hal_fs_write(out, buf, (size_t)n);
        total += n;
        if (cut_at >= 0 && total >= cut_at) break;
    }
    if (in) hal_fs_close(in);
    if (out) hal_fs_close(out);
    free(buf);
    return total;
}

static void damaged(void) {
    int64_t size = hal_fs_size("/content/.m/owner/rec/long.opus");
    CHECK(size > 10000, "the file (%lld)", (long long)size);
    /* cut off as a power failure would */
    CHECK(copy_with("/content/.m/owner/rec/long.opus", "/content/.m/owner/rec/cut2.opus", size / 2 + 7, -1) > 0, "half a file");
    int fin = g_finished;
    CHECK(AI("media.play", "{\"path\":\"/me/rec/cut2.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "it plays (%s)", g_rc.detail);
    ink_media_status_t s = st();
    CHECK(s.duration_ms > 15000 && s.duration_ms < 25000, "it knows how much there is (%u)", (unsigned)s.duration_ms);
    CHECK(BY_HAND("media.seek", "{\"ms\":14000}"), "toward the end");
    CHECK(WAIT_FOR(g_finished == fin + 1, 15000) && !strcmp(g_fin_why, "finished"), "to where it stops (%s)", g_fin_why);
    /* a damaged page is skipped, the rest plays */
    double bad0 = sim_metric_value("media.ogg_bad_page");
    CHECK(copy_with("/content/.m/owner/rec/long.opus", "/content/.m/owner/rec/bad.opus", -1, size / 2) == size, "a byte gone wrong");
    fin = g_finished;
    CHECK(AI("media.play", "{\"path\":\"/me/rec/bad.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "it plays");
    CHECK(BY_HAND("media.seek", "{\"ms\":17000}"), "just before it");
    CHECK(WAIT_FOR(st().position_ms > 24000, 15000), "and past it (%u)", (unsigned)st().position_ms);
    CHECK(sim_metric_value("media.ogg_bad_page") > bad0, "the page was seen to be bad");
    CHECK(BY_HAND("media.stop", NULL), "stop");
    /* the card pulled out while it plays */
    fin = g_finished;
    CHECK(AI("media.play", "{\"path\":\"/me/rec/long.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "it plays");
    delay_ms(1000);
    sim_sd_set_present(false);
    CHECK(WAIT_FOR(g_finished == fin + 1, 8000) && !strcmp(g_fin_why, "read_failed"), "the card goes: it ends and says why (%s)", g_fin_why);
    sim_sd_set_present(true);
    delay_ms(1500);
}

/* ---- focus ---- */

static volatile int g_ev[3];
static void focus_seen(void *ctx, ink_focus_event_t ev, ink_audio_prio_t by) {
    (void)ctx;
    (void)by;
    g_ev[ev]++;
}

static void focus_rules(void) {
    int t1 = ink_audio_focus_acquire(INK_AUDIO_PRIO_VOICE, focus_seen, NULL, INK_EXEC_DIRECT);
    CHECK(t1 > 0 && ink_audio_focus_top() == INK_AUDIO_PRIO_VOICE, "held");
    int t2 = ink_audio_focus_acquire(INK_AUDIO_PRIO_VOICE, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(t2 > 0 && g_ev[INK_FOCUS_LOST] == 1, "the same level: the newer takes over");
    int t3 = ink_audio_focus_acquire(INK_AUDIO_PRIO_ALERT, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(t3 > 0 && ink_audio_focus_top() == INK_AUDIO_PRIO_ALERT, "something higher");
    CHECK(ink_audio_focus_acquire(INK_AUDIO_PRIO_VOICE, NULL, NULL, INK_EXEC_DIRECT) == -1, "speech does not wait under it");
    /* a tone says nothing over an alarm */
    double dropped = sim_metric_value("audio.tones_dropped"), heard = sim_audio_played_ms();
    ink_audio_tone(INK_TONE_DONE, 0, NULL);
    delay_ms(500);
    CHECK(sim_metric_value("audio.tones_dropped") == dropped + 1 && sim_audio_played_ms() == heard, "a tone is dropped");
    ink_audio_focus_release(t3);
    ink_audio_focus_release(t2);
    CHECK(ink_audio_focus_top() == 0, "free again");
    ink_audio_tone(INK_TONE_DONE, 0, NULL);
    delay_ms(500);
    CHECK(sim_audio_played_ms() > heard, "and a tone is heard");
}

static void focus_and_media(void) {
    CHECK(AI("media.play", "{\"path\":\"/me/rec/long.opus\",\"start_ms\":5000}") && g_rc.status == INK_RC_DONE, "playing");
    delay_ms(2000);
    /* an alarm rings: it pauses; the ring ends: it goes on, a little earlier */
    uint32_t before = st().position_ms;
    ink_audio_ring(true);
    CHECK(WAIT_FOR(st().state == INK_MEDIA_PAUSED, 500) && st().paused_by == INK_MEDIA_BY_FOCUS, "a ring pauses it (%d, %d)", st().state, st().paused_by);
    uint32_t paused_at = st().position_ms;
    CHECK(paused_at >= before && paused_at < before + 700, "where it was (%u, %u)", (unsigned)before, (unsigned)paused_at);
    delay_ms(2000);
    ink_audio_ring(false);
    CHECK(WAIT_FOR(st().state == INK_MEDIA_PLAYING, 500), "the ring ends: it goes on by itself");
    uint32_t back = st().position_ms;
    CHECK(back <= paused_at - 2500 && back >= paused_at - 4000, "a little earlier (%u, %u)", (unsigned)paused_at, (unsigned)back);

    /* the person paused it during the ring: it stays paused */
    delay_ms(1000);
    ink_audio_ring(true);
    CHECK(WAIT_FOR(st().state == INK_MEDIA_PAUSED, 500), "again");
    CHECK(BY_HAND("media.pause", NULL), "by hand too");
    delay_ms(100);
    ink_audio_ring(false);
    delay_ms(1000);
    CHECK(st().state == INK_MEDIA_PAUSED && st().paused_by == INK_MEDIA_BY_USER, "the person's word stands (%d, %d)", st().state, st().paused_by);
    CHECK(BY_HAND("media.resume", NULL), "the person goes on");
    CHECK(st().state == INK_MEDIA_PLAYING, "playing");

    /* too long an interruption: a call of half an hour and more */
    delay_ms(1000);
    int call = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(WAIT_FOR(st().state == INK_MEDIA_PAUSED, 500), "a call pauses it");
    delay_ms(31 * 60000);
    ink_audio_focus_release(call);
    delay_ms(1000);
    CHECK(st().state == INK_MEDIA_PAUSED, "after half an hour it does not start by itself (%d)", st().state);
    CHECK(BY_HAND("media.stop", NULL), "stop");

    /* started while something else comes first: it waits */
    ink_audio_ring(true);
    CHECK(AI("media.play", "{\"path\":\"/me/rec/long.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "asked during a ring");
    CHECK(WAIT_FOR(st().state == INK_MEDIA_PAUSED, 500), "it waits");
    ink_audio_ring(false);
    CHECK(WAIT_FOR(st().state == INK_MEDIA_PLAYING, 500), "then goes");
    delay_ms(500);
}

static ink_exec_job_t g_key_job;
static void key_job(void *pressed) { ink_ai_key(pressed != NULL, osal_now_us()); }

static void ai_speaks(void) {
    /* the AI key while a book plays: it waits for the whole turn */
    ink_power_unlock(osal_now_us());
    ink_power_activity(osal_now_us());
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    CHECK(st().state == INK_MEDIA_PLAYING, "a book is playing");
    ink_exec_post(INK_EXEC_UI, &g_key_job, key_job, (void *)1);
    CHECK(WAIT_FOR(st().state == INK_MEDIA_PAUSED, 800) && st().paused_by == INK_MEDIA_BY_FOCUS, "the AI key pauses it (%d)", st().state);
    delay_ms(1200);
    ink_exec_post(INK_EXEC_UI, &g_key_job, key_job, NULL);
    double speech = sim_metric_value("ai.release_to_first_audio_heard_ms");
    CHECK(WAIT_FOR(sim_metric_value("ai.release_to_first_audio_heard_ms") > speech, 8000), "the answer is heard");
    CHECK(st().state == INK_MEDIA_PAUSED, "the book waits meanwhile");
    CHECK(WAIT_FOR(st().state == INK_MEDIA_PLAYING, 60000), "when it is over the book goes on");
    CHECK(BY_HAND("media.stop", NULL), "stop");
    delay_ms(500);

    /* an answer being spoken when an alarm rings ends */
    ink_exec_post(INK_EXEC_UI, &g_key_job, key_job, (void *)1);
    delay_ms(1200);
    ink_exec_post(INK_EXEC_UI, &g_key_job, key_job, NULL);
    speech = sim_metric_value("ai.release_to_first_audio_heard_ms");
    CHECK(WAIT_FOR(sim_metric_value("ai.release_to_first_audio_heard_ms") > speech, 8000), "another answer");
    delay_ms(300);
    ink_audio_ring(true);
    delay_ms(300);
    char now[16];
    ink_state_get_str(ink_state_find("audio.now_playing"), now, sizeof now);
    CHECK(!strcmp(now, "none"), "an alarm: what was being said ends (%s)", now);
    ink_audio_ring(false);
    delay_ms(1000);
    int f = ink_audio_focus_top();
    CHECK(f == 0, "and it does not come back (%d)", f);
}

/* ---- the headset ---- */

static void headset_gone(void *arg) {
    (void)arg;
    sim_headset_range(HEADSET, false);
}
static void headset_back(void *arg) {
    (void)arg;
    sim_headset_range(HEADSET, true);
}

static void headset(void) {
    ink_power_unlock(osal_now_us()); /* it has locked itself by now */
    ink_power_activity(osal_now_us());
    delay_ms(500);
    CHECK(bool_state("bt.available"), "this device can have one");
    CHECK(AI("bt.connect", "{\"addr\":\"" HEADSET "\"}") && g_rc.status == INK_RC_FAILED, "not before it is paired");
    CHECK(AI("bt.scan", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, HEADSET) && strstr(g_result, "测试耳机"), "found (%s)", g_result);
    CHECK(AI("bt.pair", "{\"addr\":\"" HEADSET "\",\"name\":\"测试耳机\"}") && g_rc.status == INK_RC_DONE, "paired (%s)", g_rc.detail);
    CHECK(AI("bt.list", NULL) && strstr(g_result, HEADSET) && strstr(g_result, "测试耳机"), "remembered (%s)", g_result);
    CHECK(!strcmp(state_of("audio.route"), "speaker"), "still the speaker");
    double sets = sim_metric_value("audio.bt_mode_sets");
    CHECK(AI("bt.connect", "{\"addr\":\"" HEADSET "\"}") && g_rc.status == INK_RC_DONE, "connected (%s)", g_rc.detail);
    CHECK(bool_state("bt.connected") && !strcmp(state_of("bt.device"), HEADSET), "state says so");
    CHECK(WAIT_FOR(!strcmp(state_of("audio.route"), "bt_headset"), 3000), "the sound goes there (%s)", state_of("audio.route"));
    CHECK(sim_metric_value("audio.bt_mode_sets") > sets, "through the chip's known mode command");

    double speaker = sim_audio_played_ms(), phones = sim_audio_bt_played_ms();
    CHECK(AI("media.play", "{\"path\":\"/me/rec/long.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "playing");
    delay_ms(3000);
    CHECK(sim_audio_bt_played_ms() > phones + 2000, "heard on the headset (%.0f)", sim_audio_bt_played_ms() - phones);
    CHECK(sim_audio_played_ms() == speaker, "not out loud (%.0f)", sim_audio_played_ms() - speaker);

    /* it goes out of range: back to the speaker, and not suddenly out loud */
    vtk_event_at(vtk_now() + SIM_MS(1), headset_gone, NULL);
    CHECK(WAIT_FOR(!bool_state("bt.connected"), 1000), "the link is lost");
    CHECK(WAIT_FOR(!strcmp(state_of("audio.route"), "speaker"), 3000), "the route comes back (%s)", state_of("audio.route"));
    CHECK(WAIT_FOR(st().state == INK_MEDIA_PAUSED, 1000) && st().paused_by == INK_MEDIA_BY_ROUTE, "media pauses (%d, %d)", st().state, st().paused_by);
    speaker = sim_audio_played_ms();
    delay_ms(2000);
    CHECK(sim_audio_played_ms() <= speaker + 200, "nothing out loud");
    CHECK(BY_HAND("media.stop", NULL), "stop");
    vtk_event_at(vtk_now() + SIM_MS(1), headset_back, NULL);
    delay_ms(100);

    /* Calls can switch routes (phone-2). */
    int call = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(AI("bt.connect", "{\"addr\":\"" HEADSET "\"}") && g_rc.status == INK_RC_DONE, "connected during a call (%s, %s)", ink_rc_name(g_rc.status), g_rc.detail);
    delay_ms(1500);
    CHECK(!strcmp(state_of("audio.route"), "bt_headset"), "the route switches during the call (%s)", state_of("audio.route"));
    ink_audio_focus_release(call);
    CHECK(WAIT_FOR(!strcmp(state_of("audio.route"), "bt_headset"), 3000), "the headset route stays after the call");

    CHECK(AI("bt.disconnect", NULL) && g_rc.status == INK_RC_DONE, "disconnect");
    CHECK(WAIT_FOR(!strcmp(state_of("audio.route"), "speaker"), 3000), "back");
    CHECK(AI("bt.forget", "{\"addr\":\"" HEADSET "\"}") && g_rc.status == INK_RC_DONE, "forget");
    CHECK(AI("bt.list", NULL) && !strstr(g_result, HEADSET), "gone (%s)", g_result);
    CHECK(AI("bt.connect", "{\"addr\":\"" HEADSET "\"}") && g_rc.status == INK_RC_FAILED, "it must be paired again");
}

static void volume_fixture(int volume) {
    ink_receipt_t rc;
    ink_setting_set("audio.volume", volume, INK_SRC_TOUCH, &rc);
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "volume fixture is durable before a short physical key");
}
static void volume_keys(void) {
    volume_fixture(50);
    key(HAL_KEY_VOL_UP);
    CHECK(WAIT_FOR(ink_setting_get("audio.volume") == 60, 500), "up (%d)", (int)ink_setting_get("audio.volume"));
    key(HAL_KEY_VOL_DOWN);
    key(HAL_KEY_VOL_DOWN);
    CHECK(WAIT_FOR(ink_setting_get("audio.volume") == 40, 500), "down twice (%d)", (int)ink_setting_get("audio.volume"));
    volume_fixture(95);
    key(HAL_KEY_VOL_UP);
    CHECK(WAIT_FOR(ink_setting_get("audio.volume") == 100, 500), "not past the top (%d)", (int)ink_setting_get("audio.volume"));
    volume_fixture(40);
    /* locked, nothing playing: it does not even wake */
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "locked");
    delay_ms(500);
    key(HAL_KEY_VOL_UP);
    delay_ms(300);
    CHECK(ink_setting_get("audio.volume") == 40 && !strcmp(power_state(), "locked"), "nothing to hear: nothing happens (%d, %s)", (int)ink_setting_get("audio.volume"), power_state());
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked") != 0, 70000), "unlocked");
    ink_power_activity(osal_now_us());
    /* locked, playing, the space closed: the keys work, and that is all that does */
    int d = g_pin_done;
    CHECK(ink_member_set_pin("owner", NULL, "1234", pin_told, NULL) == 0 && WAIT_FOR(g_pin_done > d, 5000) && g_pin_result == INK_PIN_OK, "a PIN");
    CHECK(setting_by_hand("member.idle_return_s", 30), "a short lock grace for the closed-space volume check");
    CHECK(BY_HAND("media.play", "{\"path\":\"/me/rec/long.opus\",\"start_ms\":0}") && g_rc.status == INK_RC_DONE, "playing");
    delay_ms(1000);
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000) && ink_member_space_open(), "locked, still within the grace");
    CHECK(WAIT_FOR(!ink_member_space_open(), 32000), "the lock grace expires");
    delay_ms(500);
    CHECK(st().state == INK_MEDIA_PLAYING, "it goes on (%d)", st().state);
    key(HAL_KEY_VOL_UP);
    CHECK(WAIT_FOR(ink_setting_get("audio.volume") == 50, 500), "the key works (%d)", (int)ink_setting_get("audio.volume"));
    CHECK(!strcmp(power_state(), "locked") && !ink_member_space_open(), "and nothing opens (%s)", power_state());
    CHECK(AI("system.set_volume", "{\"percent\":10}") && g_rc.status == INK_RC_REJECTED, "the AI still cannot (%s)", ink_rc_name(g_rc.status));
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked") != 0, 70000), "unlocked");
    ink_power_activity(osal_now_us());
    d = g_pin_done;
    CHECK(ink_member_unlock("owner", "1234", pin_told, NULL) == 0 && WAIT_FOR(g_pin_done > d, 5000) && g_pin_result == INK_PIN_OK, "in");
    CHECK(BY_HAND("media.stop", NULL), "stop");
    d = g_pin_done;
    CHECK(ink_member_set_pin("owner", "1234", NULL, pin_told, NULL) == 0 && WAIT_FOR(g_pin_done > d, 5000) && g_pin_result == INK_PIN_OK, "no PIN again");
}

static void volume(void) {
    ink_receipt_t rc;
    ink_setting_set("audio.volume", 100, INK_SRC_TOUCH, &rc);
    CHECK(setting_by_hand("audio.child_max", 50), "parent confirms the child limit");
    CHECK(ink_audio_effective_volume() == 100, "an adult: as set (%d)", ink_audio_effective_volume());
    ink_member_switch("kid");
    CHECK(ink_audio_effective_volume() == 50, "a child: the limit (%d)", ink_audio_effective_volume());
    CHECK(ink_setting_set("audio.child_max", 100, INK_SRC_TOUCH, &rc) == INK_RC_REJECTED, "which they cannot change (%s)", ink_rc_name(rc.status));
    ink_member_test_reset();
    CHECK(setting_by_hand("audio.child_max", 60), "parent confirms restoring the limit");
    ink_setting_set("audio.volume", 70, INK_SRC_TOUCH, &rc);
}

static void clean_card(void *arg) { /* what an earlier run left on the card */
    (void)arg;
    char cmd[700];
    snprintf(cmd, sizeof cmd, "rm -rf '%s/content/.m' '%s/cache/sys'", sim_sd_root(), sim_sd_root());
    int r = system(cmd);
    if (r != 0) sim_warn("cannot clean the card (%s: %d)", cmd, r);
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(2000);
    vtk_host_call(clean_card, NULL);
    CHECK(ink_cap_subscribe("media.finished", on_event, NULL, INK_EXEC_DIRECT) == 0 && ink_cap_subscribe("audio.record_done", on_event, NULL, INK_EXEC_DIRECT) == 0, "subscribe");
    recording();
    playing();
    damaged();
    focus_rules();
    focus_and_media();
    ai_speaks();
    headset();
    volume();
    volume_keys();
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "mediatest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
