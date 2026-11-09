/* Exercise member learning through the production session, capabilities and store. */
#include <stdlib.h>
#include "ink/sys/data.h"
#include "ink/sys/storage.h"
#include "ink/sys/textinput.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static bool capacity, verify;
static char key_now[48];
static ink_text_cand_t choices[60];
static int count, selected;
static const ink_db_def_t *freq;

static void host_mode(void *arg) {
    (void)arg;
    const char *mode = getenv("INK_IME_LEARNING");
    capacity = mode && strstr(mode, "capacity");
    verify = mode && strstr(mode, "verify");
}
static void query_ui(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 4095};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus synthetic text field");
    for (const char *p = key_now; *p; p++) ink_text_key(*p);
    count = ink_text_candidates(0, choices, 60);
}
static void query(const char *key) {
    snprintf(key_now, sizeof key_now, "%s", key);
    on_ui(query_ui);
}
static void first_is(const char *word) {
    CHECK(count > 0 && !strcmp(choices[0].word, word), "%.40s first: %.40s, expected %.40s", key_now,
          count > 0 ? choices[0].word : "<none>", word);
}
static int find(const char *word) {
    for (int i = 0; i < count; i++)
        if (!strcmp(choices[i].word, word)) return i;
    CHECK(false, "candidate exists: %.40s", word);
    return -1;
}
static void pick_ui(void) { ink_text_pick(selected); }
static void pick_as(const char *word, ink_source_t source) {
    selected = find(word);
    char args[64];
    snprintf(args, sizeof args, "{\"index\":%d}", selected);
    CHECK(call_as("keyboard.pick", args, source) && g_rc.status == INK_RC_DONE, "select via the shared text capability");
}
static void expect_suffix(void) {
    CHECK(!strcmp(ink_text_composing(), "y"), "selection consumes only its pinyin prefix");
}
static void switch_to(const char *member) {
    CHECK(ink_member_switch(member) == 0, "switch to %.16s", member);
    CHECK(WAIT_FOR(!ink_text_focused(), 3000), "handoff ends the previous input session");
}
static ink_db_t *open_freq(void) {
    ink_db_t *db = ink_db_open(freq, NULL);
    CHECK(db != NULL, "open this member's frequency collection");
    return db;
}
static void password_ui(void) {
    ink_text_cfg_t cfg = {.type = INK_TEXT_PASSWORD};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus password");
    for (const char *p = "xian"; *p; p++) ink_text_key(*p);
    CHECK(!*ink_text_composing() && !ink_text_candidates(0, choices, 60), "password has no pinyin candidates");
    ink_text_pick(0);
}
static void learning(void) {
    query("xian"); first_is(verify ? "西安" : "先");
    if (!verify) {
        pick_as("西安", INK_SRC_TOUCH);
        query("xian"); first_is("西安");
        query("xa"); first_is("喜爱");
        query("xiany"); pick_as("西安", INK_SRC_TOUCH); on_ui(expect_suffix);
        ink_db_t *db = open_freq();
        CHECK(ink_db_count(db) == 1, "prefix selection learns xian, without a second xiany record");
        ink_db_close(db);
        query("xa"); pick_as("西安", INK_SRC_AI);
    }
    query("xa"); first_is("西安");
    switch_to("kid");
    query("xa"); first_is("喜爱");
    query("xian"); first_is(verify ? "西安" : "先");
    if (!verify) {
        pick_as("西安", INK_SRC_AI);
        query("xian"); first_is("西安");
    }
    switch_to("owner");
    query("xian"); first_is("西安");
    query("xa"); first_is("西安");
    ink_db_t *db = open_freq();
    CHECK(ink_db_count(db) == 2, "owner has two independent pinyin/word records");
    uint32_t before = ink_db_last_ver(db);
    on_ui(password_ui);
    CHECK(AI("keyboard.pick", "{\"index\":0}") && g_rc.status == INK_RC_REJECTED, "AI cannot pick in a password");
    CHECK(ink_db_last_ver(db) == before && ink_db_count(db) == 2, "password leaves all learning unchanged");
    ink_db_close(db);
    CHECK(AI("data.collections", NULL) && !strstr(g_result, "text.user_freq"), "learning is absent from AI's collections");
    CHECK(AI("data.list", "{\"collection\":\"text.user_freq\"}") && g_rc.status == INK_RC_REJECTED, "AI cannot list learning");
    CHECK(AI("data.get", "{\"collection\":\"text.user_freq\",\"id\":1}") && g_rc.status == INK_RC_REJECTED, "AI cannot read learning");
}
static void synthetic_key(int i, char out[16]) {
    snprintf(out, 16, "synth%c%c%c", 'a' + i / 676, 'a' + i / 26 % 26, 'a' + i % 26);
}
static void query_number(int i) {
    char key[16]; synthetic_key(i, key); query(key);
}
static void no_presentation(void) { ink_text_set_presenter(NULL); }
static void fill(void) {
    /* Capacity exercises the text service; drawing 513 keyboards is unrelated. */
    on_ui(no_presentation);
    if (!verify) {
        for (int i = 0; i < 512; i++) {
            query_number(i); selected = find("测试"); on_ui(pick_ui);
        }
        ink_db_t *db = open_freq();
        CHECK(ink_db_count(db) == 512, "the declared record limit is reached");
        ink_db_close(db);
        query_number(0); selected = find("测试"); on_ui(pick_ui);
        query_number(512); selected = find("测试"); on_ui(pick_ui);
    }
    ink_db_t *db = open_freq();
    CHECK(ink_db_count(db) == 512, "one extra selection preserves the record bound");
    ink_db_cursor_t cursor = {0}; ink_rec_t r;
    size_t bytes = 0;
    while (ink_db_next(db, &cursor, &r, NULL, 0) == 1) bytes += r.len;
    CHECK(bytes <= freq->max_bytes, "payload stays within the declared byte limit");
    ink_db_close(db);
    query_number(0); first_is("测试");
    query_number(1); first_is("合成");
    query_number(511); first_is("测试");
    query_number(512); first_is("测试");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    freq = ink_db_find_def("text.user_freq");
    CHECK(freq && freq->scope == INK_DB_MEMBER && freq->max_records == 512 && freq->max_bytes == 2048 &&
          freq->drop_oldest && !freq->ai_read && !freq->ai_write, "bounded private member collection");
    query("xian");
    CHECK(WAIT_FOR(sim_metric_value("text.dict_loaded") == 1, 5000), "synthetic dictionary loads");
    if (capacity) fill();
    else learning();
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "learning and eviction are durable before restart");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_mode, NULL);
    product_main();
    osal_task_cfg_t cfg = {.name = "ime-learning-test", .fn = run, .stack_bytes = 16384,
                           .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start bounded learning driver");
}
