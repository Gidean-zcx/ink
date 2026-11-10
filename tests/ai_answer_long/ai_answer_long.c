/* Long AI answers: a ~6 KB answer streams in, pages match it word for word to the last page, an answer
 * over the device's limit ends with a clear note, and the speech of a long answer plays to its end. */
#include <stdlib.h>
#include "cJSON.h"
#include <errno.h>
#include <sys/stat.h>
#include "ink/sys/ai.h"
#include "ink/sys/audio.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char snapshot[8192], visible[4096];
static char expect_long[8192], expect_over[40960], all[40960];
static int page, pages, speech_frames;
static bool valid_panel, loaded;
static const char *scene;

/* The answers as the hub script sends them (host side). */
static void join_chunks(const cJSON *rule, char *out, size_t cap) {
    const cJSON *c;
    out[0] = 0;
    cJSON_ArrayForEach(c, cJSON_GetObjectItem(cJSON_GetObjectItem(rule, "stream"), "chunks")) {
        size_t used = strlen(out), n = strlen(cJSON_GetStringValue(c));
        if (used + n >= cap) abort();
        memcpy(out + used, cJSON_GetStringValue(c), n + 1);
    }
}
static void load_fixture(void *arg) {
    (void)arg;
    FILE *f = fopen(g_sim.hub_script, "rb");
    if (!f) return;
    static char txt[65536];
    size_t n = fread(txt, 1, sizeof txt - 1, f);
    fclose(f);
    txt[n] = 0;
    cJSON *root = cJSON_Parse(txt), *r;
    cJSON_ArrayForEach(r, cJSON_GetObjectItem(root, "rules")) {
        const char *m = cJSON_GetStringValue(cJSON_GetObjectItem(r, "method"));
        if (m && !strcmp(m, "ai.ask")) {
            join_chunks(cJSON_GetArrayItem(cJSON_GetObjectItem(r, "turns"), 0), expect_long, sizeof expect_long);
            join_chunks(cJSON_GetArrayItem(cJSON_GetObjectItem(r, "turns"), 1), expect_over, sizeof expect_over);
        } else if (m && !strcmp(m, "ai.listen")) { /* the hub's speech: frames per chunk, as sim_hub sends them */
            double cps = cJSON_GetObjectItem(cJSON_GetObjectItem(r, "listen"), "tts_chars_per_s")->valuedouble;
            const cJSON *c;
            cJSON_ArrayForEach(c, cJSON_GetObjectItem(cJSON_GetObjectItem(r, "stream"), "chunks")) {
                size_t chars = 0;
                for (const char *s = cJSON_GetStringValue(c); *s; s++) chars += ((unsigned char)*s & 0xC0) != 0x80;
                speech_frames += (int)(chars / cps / 0.06) + 1;
            }
        }
    }
    cJSON_Delete(root);
    loaded = expect_long[0] && expect_over[0] && speech_frames > 0;
}

static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static bool speaking(void) {
    char state[16] = "";
    ink_state_get_str(ink_state_find("audio.now_playing"), state, sizeof state);
    return !strcmp(state, "voice");
}
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "screen settles"); }
static void read_snapshot(void) { CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "complete AI snapshot"); }
static void parse_answer(void *arg) {
    (void)arg;
    cJSON *root = cJSON_Parse(snapshot), *f;
    valid_panel = cJSON_IsObject(root); page = pages = 0; visible[0] = 0;
    cJSON_ArrayForEach(f, cJSON_GetObjectItem(root, "fields")) {
        const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(f, "id"));
        const char *value = cJSON_GetStringValue(cJSON_GetObjectItem(f, "value"));
        if (!id || !value) continue;
        if (!strcmp(id, "answer")) snprintf(visible, sizeof visible, "%s", value);
        if (!strcmp(id, "page")) valid_panel = valid_panel && sscanf(value, "页码=%d/%d", &page, &pages) == 2;
    }
    cJSON_Delete(root);
}
static void read_answer(void) { read_snapshot(); vtk_host_call(parse_answer, NULL); }
static void host_save(void *arg) {
    (void)arg;
    char dir[600], path[650];
    int n = snprintf(dir, sizeof dir, "%s/%s", g_sim.out_dir, scene);
    if (n < 0 || (size_t)n >= sizeof dir) abort();
    if (mkdir(dir, 0700) && errno != EEXIST) abort();
    n = snprintf(path, sizeof path, "%s/snapshot.json", dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void take_picture(void *arg) { sim_display_snapshot((const char *)arg); }
static void capture(const char *name) {
    quiet(); scene = name; on_ui(read_snapshot);
    vtk_host_call(host_save, NULL);
    vtk_event_at(vtk_now() + SIM_MS(1), take_picture, (void *)name); delay_ms(5);
}

/* Every page from the first to the last, through the AI's next-page capability; returns the pages read. */
static int read_all_pages(void) {
    CHECK(AI("ai.panel.prev", NULL), "panel is there");
    while (page > 1 && AI("ai.panel.prev", NULL)) on_ui(read_answer);
    on_ui(read_answer);
    all[0] = 0;
    int total = pages, read = 0;
    for (int i = 1; i <= total && i < 1000; i++) {
        CHECK(valid_panel && page == i && pages == total && visible[0], "page %d of %d has text (got %d/%d)", i, total, page, pages);
        size_t used = strlen(all), len = strlen(visible);
        if (used + len >= sizeof all) break;
        memcpy(all + used, visible, len + 1);
        read = i;
        if (i == total) break;
        CHECK(AI("ai.panel.next", NULL) && g_rc.status == INK_RC_DONE, "next page through the capability");
        on_ui(read_answer);
    }
    return read;
}

/* An answer of nothing but line breaks, as long as the device keeps: more pages than the panel counts. */
static char lines[INK_AI_ANSWER_MAX];
static void only_lines(void) { memset(lines, '\n', sizeof lines - 1); ink_shell_ai_words(""); ink_shell_ai_words(lines); }

/* The same words laid out at once, not as they streamed in. */
static void lay_out_again(void) { ink_shell_ai_words(""); ink_shell_ai_words(expect_long); }

static const char *QUESTION;
static void ask(void) { ink_ai_ask_text(QUESTION); }

static void run(void *arg) {
    (void)arg;
    vtk_host_call(load_fixture, NULL);
    CHECK(loaded, "hub fixture read (%d speech frames)", speech_frames);
    CHECK(strlen(expect_long) >= 6000 && strlen(expect_long) < INK_AI_ANSWER_MAX, "the long answer is ~6 KB and fits");
    CHECK(strlen(expect_over) > INK_AI_ANSWER_MAX, "the other answer is over the limit");
    delay_ms(1800); quiet();
    test_start_home(); quiet();

    /* 1. ~6 KB streamed in 13 parts: while it arrives the first page only grows into its final text and stays shown */
    QUESTION = "合成长回答问题"; on_ui(ask);
    int samples = 0, last_pages = 0;
    bool calm = true;
    char first_page[4096] = "";
    while (sim_metric_value("ai.answers") < 1 && samples < 400) {
        delay_ms(70);
        on_ui(read_answer);
        if (!valid_panel || !visible[0] || strstr(visible, "合成长回答问题")) continue;
        samples++;
        if (page != 1 || pages < last_pages || strncmp(expect_long, visible, strlen(visible))) calm = false;
        if (pages > 1 && !first_page[0]) snprintf(first_page, sizeof first_page, "%s", visible);
        else if (first_page[0] && strcmp(first_page, visible)) calm = false;
        last_pages = pages;
    }
    CHECK(samples >= 8, "the answer was watched while it streamed (%d samples)", samples);
    CHECK(calm, "while streaming: page 1 stays, the page count only grows, the text is the answer's beginning");
    CHECK(first_page[0], "the answer outgrew one page while streaming");
    capture("AiLong6k");
    on_ui(read_answer);
    CHECK(pages > 5 && page == 1, "6 KB answer is paged (%d pages)", pages);
    int long_pages = pages;
    int read = read_all_pages();
    CHECK(read == long_pages && page == pages, "reached the last page (%d of %d)", read, long_pages);
    CHECK(!strcmp(all, expect_long), "pages joined equal the answer word for word (%zu of %zu bytes)", strlen(all), strlen(expect_long));
    CHECK(!strstr(all, "回答太长"), "a 6 KB answer is not cut");
    capture("AiLong6kLast");
    on_ui(lay_out_again); on_ui(read_answer);
    CHECK(pages == long_pages, "streamed pages break where pages laid out at once do (%d, %d)", pages, long_pages);
    CHECK(read_all_pages() == long_pages && !strcmp(all, expect_long), "and hold the same words");
    CHECK(sim_metric_value("ai.answer_cut") == 0, "nothing cut so far");

    /* 2. over the limit: the beginning is kept word for word and the end says the answer was too long */
    QUESTION = "合成超长回答问题"; on_ui(ask);
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") >= 2, 30000), "over-long answer finished");
    quiet(); on_ui(read_answer);
    read = read_all_pages();
    size_t got = strlen(all), cut = strlen(INK_AI_ANSWER_CUT);
    CHECK(read == pages && got > cut && !strcmp(all + got - cut, INK_AI_ANSWER_CUT), "the last page ends with the note (%d pages)", pages);
    size_t kept = got > cut ? got - cut : 0;
    CHECK(kept < INK_AI_ANSWER_MAX && kept + cut + 4 >= INK_AI_ANSWER_MAX - 1, "kept up to the limit (%zu bytes)", kept);
    CHECK(!memcmp(all, expect_over, kept) && ((unsigned char)expect_over[kept] & 0xC0) != 0x80, "kept part is the answer's beginning, cut at a whole character");
    CHECK(sim_metric_value("ai.answer_cut") == 1, "the cut is counted once");
    capture("AiLongCut");
    CHECK(strstr(snapshot, "回答太长，只显示了前面部分"), "the AI snapshot shows the note");
    on_ui(only_lines); on_ui(read_answer);
    CHECK(strstr(visible, "回答无法完整显示"), "32 KB of line breaks: the panel says it cannot show it in full (%.60s, %d pages)", visible, pages);

    /* 3. voice: the speech of the 6 KB answer (more frames than the jitter buffer holds, sent in real time as the
     * hub does) plays to the end at normal speed, well beyond the initial 45 s wait. */
    double heard = sim_audio_played_ms();
    CHECK(AI("ai.panel.listen", NULL) && g_rc.status == INK_RC_DONE, "tap to ask by voice");
    delay_ms(1500);
    CHECK(AI("ai.panel.listen", NULL) && g_rc.status == INK_RC_DONE, "second tap ends the question");
    CHECK(WAIT_FOR(speaking(), 20000), "speech starts");
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") >= 3 && !speaking(), 600000), "speech ends by itself");
    double played = sim_audio_played_ms() - heard, want = speech_frames * 60.0;
    CHECK(speech_frames > 512 && played >= 0.97 * want, "whole speech heard: %.0f of %.0f ms (%d frames)", played, want, speech_frames);
    CHECK(sim_metric_value("ai.downlink_overflow") == 0 && sim_metric_value("audio.jitter_overflow") == 0 &&
          sim_metric_value("ai.speech_failed") == 0, "no speech frame dropped");
    quiet(); on_ui(read_answer);
    CHECK(pages == long_pages, "the spoken answer shows on the same pages (%d)", pages);
    read = read_all_pages();
    CHECK(read == long_pages && !strcmp(all, expect_long), "spoken answer's pages equal its text");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "ai-answer-long", .fn = run, .stack_bytes = 24576, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start scenario driver");
}
