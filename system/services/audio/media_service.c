/*
 * Media playback and recording (see ink/sys/media.h). The media executor reads the file and keeps about two seconds ahead
 * in the audio service's queue; it never touches the UI task. Where it was goes to the flash when playback stops or pauses,
 * and to the card every half minute while it plays: a flash write would stop the sound, a card write does not (foundation
 * §5.5.1); the newer of the two counts next time.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/sys/cap.h"
#include "ink/sys/crc.h"
#include "ink/sys/data.h"
#include "ink/sys/file.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/media.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "ink/sys/time.h"
#include "ogg.h"

#define AHEAD_FRAMES 34          /* about 2 s */
#define PUMP_US 500000ull
#define POS_TO_CARD_US 30000000ull
#define RESUME_WITHIN_S 1800     /* an interruption longer than half an hour: it stays paused */
#define RESUME_BACK_MS 3000      /* it goes on a little earlier */
#define POS_CACHE "/cache/sys/media-pos"
#define REC_FRAMES 128           /* about 8 s between the audio task and the writer */
#define REC_SYNC_MS 5000
#define REC_MAX_S (6 * 3600)

static const ink_db_def_t POSITIONS = {.name = "media.positions", .title = "听到哪儿了", .scope = INK_DB_MEMBER, .schema = 1, .json = true, .ai_read = true, .max_records = 100, .drop_oldest = true};

static struct {
    /* the status others read, under the service lock */
    ink_media_status_t st;
    /* the executor's */
    ogg_reader_t *r;
    int stream;
    uint32_t base_ms;      /* the stream's first frame is here in the file */
    bool at_end;
    int focus;
    uint64_t paused_at_s;
    uint64_t last_card_us;
    char member[INK_MEMBER_ID_MAX];
    ink_svc_deadline_t pump_at;
    ink_exec_job_t pump_job, member_job, route_job, battery_job;
    int h_state;
} M INK_PSRAM_BSS;

typedef struct {
    ink_exec_job_t job;
    void (*fn)(uint32_t arg, const char *path);
    uint32_t arg;
    char path[INK_FS_PATH_MAX];
} media_cmd_t;

static const char *const STATE_NAME[] = {"idle", "playing", "paused"};

static void set_state(ink_media_state_t s, ink_media_pause_t by) {
    ink_svc_lock();
    M.st.state = s;
    M.st.paused_by = by;
    ink_svc_unlock();
    ink_state_set_str(M.h_state, STATE_NAME[s]);
}

static void set_position(uint32_t ms) {
    ink_svc_lock();
    M.st.position_ms = ms;
    ink_svc_unlock();
}

/* Where the listener is now: what has been played of the stream, as far as it came. */
static uint32_t heard_ms(void) {
    uint32_t played = 0;
    if (M.stream) ink_audio_stream_progress(M.stream, &played, NULL);
    return M.base_ms + played * OGG_FRAME_MS;
}

/* ---- where it was ---- */

static uint32_t path_id(const char *path) {
    uint32_t id = ink_crc32(0, path, strlen(path)) & 0x7fffffff;
    return id ? id : 1;
}

static void save_to_flash(const char *path, uint32_t ms) {
    ink_db_t *db = ink_db_open(&POSITIONS, M.member);
    if (!db) return;
    char buf[INK_FS_PATH_MAX + 64];
    ink_jw_t w;
    ink_jw_init(&w, buf, sizeof buf);
    ink_jw_raw(&w, "{\"path\":");
    ink_jw_str(&w, path);
    ink_jw_raw(&w, ",\"ms\":");
    ink_jw_int(&w, ms);
    ink_jw_raw(&w, "}");
    ink_db_write_t rec = {.id = path_id(path), .sort = ink_time_now_s(), .data = buf, .len = w.len, .src = INK_SRC_SYSTEM};
    if (!w.overflow) ink_db_put(db, &rec, NULL);
    ink_db_close(db);
    ink_probe_count("media.positions_saved");
}

static void save_to_card(const char *path, uint32_t ms) {
    char buf[INK_FS_PATH_MAX + 64];
    int n = snprintf(buf, sizeof buf, "%lld\n%s\n%s\n%u\n", (long long)ink_time_now_s(), M.member, path, (unsigned)ms);
    if (ink_fs_write_atomic(POS_CACHE, buf, (size_t)n) == 0) ink_probe_count("media.positions_cached");
}

/* The newer of what the flash and the card say. */
static uint32_t remembered(const char *path) {
    int64_t flash_at = -1;
    uint32_t flash_ms = 0, card_ms = 0;
    ink_db_t *db = ink_db_open(&POSITIONS, M.member);
    if (db) {
        char buf[INK_FS_PATH_MAX + 64];
        ink_rec_t r;
        if (ink_db_get(db, path_id(path), &r, buf, sizeof buf - 1) == INK_DB_OK) {
            buf[r.len < sizeof buf - 1 ? r.len : sizeof buf - 1] = 0;
            const char *m = strstr(buf, "\"ms\":");
            if (m) flash_ms = (uint32_t)strtoul(m + 5, NULL, 10);
            flash_at = r.sort;
        }
        ink_db_close(db);
    }
    ink_fs_file_t *f;
    if (ink_fs_open(POS_CACHE, INK_FS_READ, &f) == 0) {
        char buf[INK_FS_PATH_MAX + 64];
        int64_t n = ink_fs_read(f, buf, sizeof buf - 1);
        ink_fs_close(f);
        if (n > 0) {
            buf[n] = 0;
            char *who = strchr(buf, '\n');
            char *what = who ? strchr(who + 1, '\n') : NULL;
            char *end = what ? strchr(what + 1, '\n') : NULL;
            if (end) {
                *who = *what = *end = 0;
                int64_t at = strtoll(buf, NULL, 10);
                if (!strcmp(who + 1, M.member) && !strcmp(what + 1, path) && at > flash_at) {
                    card_ms = (uint32_t)strtoul(end + 1, NULL, 10);
                    return card_ms;
                }
            }
        }
    }
    return flash_at >= 0 ? flash_ms : 0;
}

/* ---- playing ---- */

static void pump(void *arg);

static void pump_due(void *arg) { /* the timer task */
    (void)arg;
    ink_exec_post(INK_EXEC_MEDIA, &M.pump_job, pump, NULL);
}

static void close_stream(void) {
    if (M.stream) ink_audio_stream_abort(M.stream);
    M.stream = 0;
}

static void end_session(const char *why) { /* executor */
    ink_svc_cancel(&M.pump_at);
    close_stream();
    if (M.focus) ink_audio_focus_release(M.focus);
    M.focus = 0;
    ogg_close(M.r);
    M.r = NULL;
    char path[INK_FS_PATH_MAX];
    ink_svc_lock();
    snprintf(path, sizeof path, "%s", M.st.path);
    ink_svc_unlock();
    set_state(INK_MEDIA_IDLE, INK_MEDIA_BY_USER);
    if (why) {
        ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
        if (a) {
            ink_args_init(a);
            ink_args_str(a, "path", path);
            ink_args_str(a, "why", why);
            ink_cap_emit("media.finished", a);
            osal_free(a);
        }
    }
}

/* The stream starts at the reader's place. */
static bool open_stream(void) {
    M.stream = ink_audio_stream_open(INK_AUDIO_PRIO_STORY, 0, NULL);
    if (M.stream <= 0) {
        M.stream = 0;
        return false;
    }
    M.base_ms = ogg_position_ms(M.r);
    M.at_end = false;
    return true;
}

static void pump(void *arg) { /* executor: keep the queue filled, see how it goes */
    (void)arg;
    if (!M.r || M.st.state != INK_MEDIA_PLAYING) return;
    uint32_t played = 0, queued = 0;
    if (!M.stream && !open_stream()) { /* something else has the speaker: the focus says when */
        ink_svc_at(&M.pump_at, osal_now_us() + PUMP_US, pump_due, NULL);
        return;
    }
    if (ink_audio_stream_progress(M.stream, &played, &queued) < 0) { /* the stream has gone */
        M.stream = 0;
        if (M.at_end) {
            set_position(ogg_duration_ms(M.r));
            ink_probe_count("media.finished");
            char path[INK_FS_PATH_MAX];
            snprintf(path, sizeof path, "%s", M.st.path);
            save_to_flash(path, 0); /* heard to the end: next time from the start */
            end_session("finished");
        } else if (ink_audio_focus_top() > INK_AUDIO_PRIO_STORY) {
            ink_svc_at(&M.pump_at, osal_now_us() + PUMP_US, pump_due, NULL);
        } else { /* the speaker failed, or someone took it without saying */
            ink_probe_count("media.stream_lost");
            end_session("output_failed");
        }
        return;
    }
    while (queued < AHEAD_FRAMES && !M.at_end) {
        const uint8_t *data;
        size_t len;
        int n = ogg_next_packet(M.r, &data, &len);
        if (n < 0) { /* the card went away, or it cannot be read */
            ink_probe_count("media.read_failed");
            char path[INK_FS_PATH_MAX];
            snprintf(path, sizeof path, "%s", M.st.path);
            save_to_flash(path, heard_ms());
            end_session("read_failed");
            return;
        }
        if (n == 0) {
            M.at_end = true;
            ink_audio_stream_finish(M.stream);
            break;
        }
        if (ink_audio_stream_push(M.stream, data, len) != 0) break;
        queued++;
    }
    uint32_t now_ms = M.base_ms + played * OGG_FRAME_MS;
    set_position(now_ms);
    uint64_t now = osal_now_us();
    if (now - M.last_card_us >= POS_TO_CARD_US) {
        M.last_card_us = now;
        char path[INK_FS_PATH_MAX];
        snprintf(path, sizeof path, "%s", M.st.path);
        save_to_card(path, now_ms);
    }
    ink_svc_at(&M.pump_at, now + PUMP_US, pump_due, NULL);
}

static void do_pause(ink_media_pause_t by) { /* executor */
    if (M.st.state != INK_MEDIA_PLAYING) {
        if (M.st.state == INK_MEDIA_PAUSED && by == INK_MEDIA_BY_USER) set_state(INK_MEDIA_PAUSED, by); /* the person's word is the last */
        return;
    }
    uint32_t at = heard_ms();
    ink_svc_cancel(&M.pump_at);
    close_stream();
    set_position(at);
    set_state(INK_MEDIA_PAUSED, by);
    M.paused_at_s = (uint64_t)ink_time_now_s();
    char path[INK_FS_PATH_MAX];
    snprintf(path, sizeof path, "%s", M.st.path);
    save_to_flash(path, at);
    ink_probe_count("media.paused");
}

static void do_resume(uint32_t back_ms) { /* executor */
    if (M.st.state != INK_MEDIA_PAUSED || !M.r) return;
    uint32_t at = M.st.position_ms > back_ms ? M.st.position_ms - back_ms : 0;
    uint32_t got = ogg_seek(M.r, at);
    set_position(got);
    set_state(INK_MEDIA_PLAYING, INK_MEDIA_BY_USER);
    ink_probe_count("media.resumed");
    pump(NULL);
}

static void on_focus(void *ctx, ink_focus_event_t ev, ink_audio_prio_t by) { /* executor */
    (void)ctx;
    (void)by;
    if (ev == INK_FOCUS_PAUSED) do_pause(INK_MEDIA_BY_FOCUS);
    else if (ev == INK_FOCUS_LOST) {
        M.focus = 0;
        do_pause(INK_MEDIA_BY_USER);
    } else if (M.st.state == INK_MEDIA_PAUSED && M.st.paused_by == INK_MEDIA_BY_FOCUS) {
        if ((uint64_t)ink_time_now_s() - M.paused_at_s <= RESUME_WITHIN_S) do_resume(RESUME_BACK_MS); /* it needs a word to catch up */
        else ink_probe_count("media.too_long_to_resume");
    }
}

static int do_play(const char *path, uint32_t start_ms) { /* executor; 0 or an error (ink_fs_err_t, or -100 not a playable file) */
    if (M.r) {
        char was[INK_FS_PATH_MAX];
        snprintf(was, sizeof was, "%s", M.st.path);
        save_to_flash(was, heard_ms());
        end_session(NULL);
    }
    int fs_err = 0;
    ogg_reader_t *r = NULL;
    int e = ogg_open(path, &r, &fs_err);
    if (e == OGG_ERR_FILE) return fs_err;
    if (e != OGG_OK) return -100;
    M.r = r;
    snprintf(M.member, sizeof M.member, "%s", ink_member_current()->id);
    ink_svc_lock();
    snprintf(M.st.path, sizeof M.st.path, "%s", path);
    M.st.duration_ms = ogg_duration_ms(r);
    ink_svc_unlock();
    if (start_ms == INK_MEDIA_RESUME) start_ms = remembered(path);
    if (start_ms >= M.st.duration_ms) start_ms = 0; /* heard to the end */
    uint32_t got = start_ms ? ogg_seek(r, start_ms) : 0;
    set_position(got);
    M.last_card_us = osal_now_us();
    M.stream = 0;
    M.focus = ink_audio_focus_acquire(INK_AUDIO_PRIO_STORY, on_focus, NULL, INK_EXEC_MEDIA);
    set_state(INK_MEDIA_PLAYING, INK_MEDIA_BY_USER);
    ink_probe_count("media.started");
    pump(NULL);
    return 0;
}

static void do_seek(uint32_t ms) { /* executor */
    if (!M.r) return;
    bool playing = M.st.state == INK_MEDIA_PLAYING;
    if (ms > M.st.duration_ms) ms = M.st.duration_ms;
    ink_svc_cancel(&M.pump_at);
    close_stream();
    uint32_t got = ogg_seek(M.r, ms);
    set_position(got);
    char path[INK_FS_PATH_MAX];
    snprintf(path, sizeof path, "%s", M.st.path);
    save_to_flash(path, got);
    if (playing) pump(NULL);
}

static void do_stop(void) { /* executor */
    if (!M.r) return;
    char path[INK_FS_PATH_MAX];
    snprintf(path, sizeof path, "%s", M.st.path);
    save_to_flash(path, heard_ms());
    end_session("stopped");
}

/* ---- from any task ---- */

static void cmd_job(void *arg) {
    media_cmd_t *c = arg;
    c->fn(c->arg, c->path);
    osal_free(c);
}

static int post_cmd(void (*fn)(uint32_t, const char *), uint32_t a, const char *path) {
    media_cmd_t *c = osal_calloc(1, sizeof *c, OSAL_MEM_EXTERNAL);
    if (!c) return -1;
    c->fn = fn;
    c->arg = a;
    if (path) snprintf(c->path, sizeof c->path, "%s", path);
    if (ink_exec_post(INK_EXEC_MEDIA, &c->job, cmd_job, c) != 0) {
        osal_free(c);
        return -1;
    }
    return 0;
}

static void c_play(uint32_t a, const char *p) { do_play(p, a); }
static void c_pause(uint32_t a, const char *p) {
    (void)a;
    (void)p;
    do_pause(INK_MEDIA_BY_USER);
}
static void c_resume(uint32_t a, const char *p) {
    (void)a;
    (void)p;
    do_resume(0);
}
static void c_seek(uint32_t a, const char *p) {
    (void)p;
    do_seek(a);
}
static void c_stop(uint32_t a, const char *p) {
    (void)a;
    (void)p;
    do_stop();
}

int ink_media_play(const char *path, uint32_t start_ms) { return post_cmd(c_play, start_ms, path); }
void ink_media_pause(void) { post_cmd(c_pause, 0, NULL); }
void ink_media_resume(void) { post_cmd(c_resume, 0, NULL); }
int ink_media_seek(uint32_t ms) { return post_cmd(c_seek, ms, NULL); }
void ink_media_stop(void) { post_cmd(c_stop, 0, NULL); }

void ink_media_status(ink_media_status_t *out) {
    ink_svc_lock();
    *out = M.st;
    ink_svc_unlock();
}

/* ---- what else stops it ---- */

static void member_job(void *arg) { /* someone else: it stays with the one who was listening */
    (void)arg;
    do_stop();
}

static void on_member(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)h;
    (void)id;
    ink_exec_post(INK_EXEC_MEDIA, &M.member_job, member_job, NULL);
}

static void route_job(void *arg) { /* the headset went: not suddenly out loud */
    (void)arg;
    if (ink_audio_route() == INK_ROUTE_SPEAKER) do_pause(INK_MEDIA_BY_ROUTE);
}

static void on_route(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)h;
    (void)id;
    ink_exec_post(INK_EXEC_MEDIA, &M.route_job, route_job, NULL);
}

/* ---- recording ---- */

typedef struct {
    uint8_t *data;
    uint16_t len;
} rec_frame_t;

static struct {
    bool on, ending;     /* under the service lock */
    ogg_writer_t *w;
    char path[INK_FS_PATH_MAX];
    rec_frame_t q[REC_FRAMES];
    uint32_t head, tail; /* under the service lock */
    uint32_t last_sync_ms, max_ms;
    int focus;
    int dropped;
    char why[24];
    ink_exec_job_t write_job, stop_job;
    int h_recording;
} REC INK_PSRAM_BSS;

bool ink_audio_recording(void) {
    ink_svc_lock();
    bool on = REC.on;
    ink_svc_unlock();
    return on;
}

static void rec_finish(const char *why) { /* executor */
    uint32_t ms = ogg_written_ms(REC.w);
    int r = ogg_finish(REC.w);
    REC.w = NULL;
    if (REC.focus) ink_audio_focus_release(REC.focus);
    REC.focus = 0;
    ink_svc_lock();
    REC.on = REC.ending = false;
    ink_svc_unlock();
    ink_state_set_bool(REC.h_recording, false);
    ink_probe_count("media.recordings");
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (a) {
        ink_args_init(a);
        ink_args_str(a, "path", REC.path);
        ink_args_int(a, "ms", ms);
        ink_args_str(a, "why", r && !REC.why[0] ? "write_failed" : why);
        ink_cap_emit("audio.record_done", a);
        osal_free(a);
    }
}

static void rec_write(void *arg) { /* executor: what the microphone gave, to the card */
    (void)arg;
    if (!REC.w) return;
    for (;;) {
        rec_frame_t f = {0};
        ink_svc_lock();
        bool any = REC.tail != REC.head;
        if (any) {
            f = REC.q[REC.tail % REC_FRAMES];
            REC.tail++;
        }
        bool ending = REC.ending;
        ink_svc_unlock();
        if (!any) {
            if (ending) rec_finish(REC.why[0] ? REC.why : "stopped");
            return;
        }
        if (!f.data) { /* the end of the capture, in its place in the queue */
            ink_svc_lock();
            REC.ending = true;
            ink_svc_unlock();
            continue;
        }
        int e = ogg_write_packet(REC.w, f.data, f.len, OGG_FRAME_MS);
        osal_free(f.data);
        if (e) { /* full, or the card went */
            snprintf(REC.why, sizeof REC.why, "%s", e == INK_FS_ERR_NO_SPACE || e == INK_FS_ERR_IO ? "no_space" : "write_failed"); /* a card that is there and takes no more */
            ink_audio_capture_end();
            continue;
        }
        uint32_t ms = ogg_written_ms(REC.w);
        if (ms - REC.last_sync_ms >= REC_SYNC_MS) {
            REC.last_sync_ms = ms;
            ogg_sync(REC.w);
        }
        if (REC.max_ms && ms >= REC.max_ms && !REC.why[0]) {
            snprintf(REC.why, sizeof REC.why, "max_length");
            ink_audio_capture_end();
        }
    }
}

static void rec_frame(void *ctx, const uint8_t *data, size_t len, uint32_t seq) { /* the audio task: short */
    (void)ctx;
    (void)seq;
    uint8_t *copy = NULL;
    if (len) {
        copy = osal_malloc(len, OSAL_MEM_EXTERNAL);
        if (!copy) {
            ink_probe_count("media.record_lost_frames");
            return;
        }
        memcpy(copy, data, len);
    }
    ink_svc_lock();
    bool room = REC.head - REC.tail < REC_FRAMES;
    if (room) REC.q[REC.head++ % REC_FRAMES] = (rec_frame_t){copy, (uint16_t)len};
    ink_svc_unlock();
    if (!room) {
        osal_free(copy);
        ink_probe_count("media.record_lost_frames");
    }
    ink_exec_post(INK_EXEC_MEDIA, &REC.write_job, rec_write, NULL);
}

static void rec_on_focus(void *ctx, ink_focus_event_t ev, ink_audio_prio_t by) { /* something comes first: what there is, is kept */
    (void)ctx;
    (void)by;
    if (ev != INK_FOCUS_LOST || !ink_audio_recording()) return;
    REC.focus = 0;
    snprintf(REC.why, sizeof REC.why, "interrupted");
    ink_audio_capture_end();
}

/* ---- on the bus ---- */

static void fail(ink_call_t *c, ink_rc_t st, const char *why) {
    c->rc.status = st;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}

static void cap_play(ink_call_t *c) { /* executor */
    char path[INK_FS_PATH_MAX];
    int64_t start = -1;
    ink_args_get_str(&c->args, "path", path, sizeof path);
    ink_args_get_int(&c->args, "start_ms", &start);
    int e = do_play(path, start < 0 ? INK_MEDIA_RESUME : (uint32_t)start);
    if (e == -100) fail(c, INK_RC_FAILED, "not an Ogg Opus file (one channel)");
    else if (e) fail(c, INK_RC_FAILED, ink_fs_err_name(e));
    else {
        snprintf(c->rc.object, sizeof c->rc.object, "media:%.40s", path);
        ink_media_status_t st;
        ink_media_status(&st);
        snprintf(c->rc.say, sizeof c->rc.say, "从第 %u 秒开始放", (unsigned)(st.position_ms / 1000));
    }
}

static void need_session(ink_call_t *c, void (*act)(void)) {
    if (!M.r) fail(c, INK_RC_FAILED, "nothing is playing");
    else act();
}

static void act_pause(void) { do_pause(INK_MEDIA_BY_USER); }
static void act_resume(void) { do_resume(0); }
static void act_stop(void) { do_stop(); }
static void cap_pause(ink_call_t *c) { need_session(c, act_pause); }
static void cap_resume(ink_call_t *c) { need_session(c, act_resume); }
static void cap_stop(ink_call_t *c) { need_session(c, act_stop); }

static void cap_seek(ink_call_t *c) {
    int64_t ms = 0;
    ink_args_get_int(&c->args, "ms", &ms);
    if (!M.r) fail(c, INK_RC_FAILED, "nothing is playing");
    else do_seek((uint32_t)ms);
}

static void cap_status(ink_call_t *c) {
    ink_media_status_t st;
    ink_media_status(&st);
    static const char *const BY[] = {"user", "focus", "route"};
    char out[256];
    ink_jw_t w;
    ink_jw_init(&w, out, sizeof out);
    ink_jw_raw(&w, "{\"state\":");
    ink_jw_str(&w, STATE_NAME[st.state]);
    if (st.state != INK_MEDIA_IDLE) {
        ink_jw_raw(&w, ",\"path\":");
        ink_jw_str(&w, st.path);
        ink_jw_raw(&w, ",\"position_ms\":");
        ink_jw_int(&w, st.position_ms);
        ink_jw_raw(&w, ",\"duration_ms\":");
        ink_jw_int(&w, st.duration_ms);
        if (st.state == INK_MEDIA_PAUSED) {
            ink_jw_raw(&w, ",\"paused_by\":");
            ink_jw_str(&w, BY[st.paused_by]);
        }
    }
    ink_jw_raw(&w, "}");
    ink_cap_result(c, out);
}

static void cap_record(ink_call_t *c) { /* executor */
    char path[INK_FS_PATH_MAX] = "";
    int64_t max_s = 0;
    ink_args_get_str(&c->args, "path", path, sizeof path);
    ink_args_get_int(&c->args, "max_seconds", &max_s);
    if (ink_audio_recording()) {
        fail(c, INK_RC_FAILED, "already recording");
        return;
    }
    if (ink_audio_focus_top() >= INK_AUDIO_PRIO_VOICE) { /* the AI's turn or a call is not ended by a recording starting */
        fail(c, INK_RC_FAILED, "busy: the AI is answering, or a call or an alarm comes first");
        return;
    }
    if (!path[0]) {
        ink_tm_t t;
        ink_time_to_local(ink_time_now_s(), &t);
        snprintf(path, sizeof path, "/me/recordings/%04d%02d%02d-%02d%02d%02d.opus", t.year, t.month, t.day, t.hour, t.min, t.sec);
    }
    char dir[INK_FS_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash && slash != dir) {
        *slash = 0;
        ink_fs_mkdir(dir);
    }
    REC.focus = ink_audio_focus_acquire(INK_AUDIO_PRIO_VOICE, rec_on_focus, NULL, INK_EXEC_MEDIA);
    if (REC.focus < 0) {
        REC.focus = 0;
        fail(c, INK_RC_FAILED, "a call or an alarm comes first");
        return;
    }
    int fs_err = 0;
    int e = ogg_create(path, &REC.w, &fs_err);
    if (e != OGG_OK) {
        ink_audio_focus_release(REC.focus);
        REC.focus = 0;
        fail(c, INK_RC_FAILED, e == OGG_ERR_FILE ? ink_fs_err_name(fs_err) : "out of memory");
        return;
    }
    snprintf(REC.path, sizeof REC.path, "%s", path);
    REC.why[0] = 0;
    REC.last_sync_ms = 0;
    REC.max_ms = max_s > 0 && max_s < REC_MAX_S ? (uint32_t)max_s * 1000u : REC_MAX_S * 1000u;
    ink_svc_lock();
    REC.head = REC.tail = 0;
    REC.ending = false;
    REC.on = true;
    ink_svc_unlock();
    if (ink_audio_capture_begin(rec_frame, NULL) != 0) { /* the AI has the microphone, or there is none */
        ink_svc_lock();
        REC.on = false;
        ink_svc_unlock();
        ogg_finish(REC.w);
        REC.w = NULL;
        ink_fs_remove(path);
        ink_audio_focus_release(REC.focus);
        REC.focus = 0;
        fail(c, INK_RC_FAILED, "the microphone is in use");
        return;
    }
    ink_state_set_bool(REC.h_recording, true);
    snprintf(c->rc.object, sizeof c->rc.object, "file:%.40s", path);
    char result[INK_FS_PATH_MAX + 16];
    snprintf(result, sizeof result, "{\"path\":\"%s\"}", path); /* our own name or the file service's: no quotes in it */
    ink_cap_result(c, result);
}

static void cap_record_stop(ink_call_t *c) {
    if (!ink_audio_recording()) {
        fail(c, INK_RC_FAILED, "not recording");
        return;
    }
    ink_audio_capture_end();
}

#define MEDIA_CAP(id_, title_, run_) {.id = id_, .title = title_, .effect = INK_FX_NAVIGATE, .child_ok = true, .exec = INK_EXEC_MEDIA, .run = run_}

static const ink_cap_t MEDIA_CAPS[] = {
    {.id = "media.play", .title = "放一个音频文件（Ogg Opus），不给位置就从上次听到的地方",
     .input = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127},\"start_ms\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"path\"]}",
     .effect = INK_FX_NAVIGATE, .child_ok = true, .permission = "media", .exec = INK_EXEC_MEDIA, .run = cap_play},
    MEDIA_CAP("media.pause", "暂停", cap_pause),
    MEDIA_CAP("media.resume", "接着放", cap_resume),
    {.id = "media.seek", .title = "跳到某个位置", .input = "{\"type\":\"object\",\"properties\":{\"ms\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"ms\"]}",
     .effect = INK_FX_NAVIGATE, .child_ok = true, .exec = INK_EXEC_MEDIA, .run = cap_seek},
    MEDIA_CAP("media.stop", "停止", cap_stop),
    {.id = "media.status", .title = "现在放的是什么，到哪儿了", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_status},
    {.id = "media.finished", .title = "播放结束了（放完、停止、读不了）", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    /* a microphone turned on by someone else is for the person to agree to */
    {.id = "audio.record", .title = "开始录音，存成文件", .ask = "开始录音",
     .input = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127},\"max_seconds\":{\"type\":\"integer\",\"minimum\":1}}}",
     .output = "{\"type\":\"object\"}", .effect = INK_FX_EXTERNAL, .child_ok = true, .permission = "record", .exec = INK_EXEC_MEDIA, .run = cap_record},
    {.id = "audio.record_stop", .title = "停止录音", .effect = INK_FX_NAVIGATE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_record_stop},
    {.id = "audio.record_done", .title = "录音结束了（存好的文件、时长、为什么停）", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

static const ink_state_def_t ST_MEDIA = {.id = "media.state", .type = INK_ST_STR, .title = "播放状态（idle、playing、paused）"};
static const ink_state_def_t ST_RECORDING = {.id = "audio.recording", .type = INK_ST_BOOL, .title = "正在录音"};

int ink_media_init(void) {
    M.h_state = ink_state_define(&ST_MEDIA);
    ink_state_set_str(M.h_state, "idle");
    REC.h_recording = ink_state_define(&ST_RECORDING);
    ink_state_set_bool(REC.h_recording, false);
    if (ink_db_register(&POSITIONS) != 0 || ink_state_watch("member.current", on_member, NULL, INK_EXEC_DIRECT) != 0 || ink_state_watch("audio.route", on_route, NULL, INK_EXEC_DIRECT) != 0) return -1;
    return ink_cap_register(MEDIA_CAPS, (int)(sizeof MEDIA_CAPS / sizeof MEDIA_CAPS[0]));
}
