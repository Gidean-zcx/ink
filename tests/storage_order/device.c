/* Storage ordering and physical input on the full simulated product; synthetic data only. */
#include "../services_finish/product.h"
#include "ink/sys/ai.h"

unsigned ink_test_index_count(void);
unsigned ink_test_index_writes(void);
size_t ink_test_pending_bytes(ink_db_t *db);
size_t ink_test_pending_limit(void);
static char mode[24];
static int key_count, touch_count;
static bool ready(void) {
    char s[32];
    return ink_state_get_str(ink_state_find("file.index_state"), s, sizeof s) && !strcmp(s, "ready");
}
static void clicked(lv_event_t *e) { (void)e; touch_count++; }
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    lv_obj_t *button = lv_button_create(screen);
    lv_obj_set_size(button, 300, 300);
    lv_obj_center(button);
    lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, NULL);
}
static bool pressed(ink_key_t key, bool long_press) {
    if (key != INK_KEY_NEXT || long_press) return false;
    key_count++;
    return true;
}
static const ink_activity_t INPUT = {.name = "SYNTH input", .create = create, .on_key = pressed};
static void open_input(void) { ink_ui_push(&INPUT, NULL); }
static void pair(void *arg) {
    bool down = arg != NULL;
    sim_input_key(HAL_KEY_NEXT, down);
    sim_input_touch(0, down, 342, 608);
}
static void interactions(void) {
    test_start_home();
    on_ui(open_input);
    CHECK(WAIT_FOR(ink_test_index_count() >= 32 && !ready(), 5000), "physical inputs overlap automatic indexing");
    unsigned before = ink_test_index_count();
    for (int i = 0; i < 10; i++) {
        uint64_t now = vtk_now();
        vtk_event_at(now + SIM_MS(1), pair, (void *)1);
        vtk_event_at(now + SIM_MS(81), pair, NULL);
        delay_ms(160);
    }
    CHECK(key_count == 10, "ten short physical keys arrive: %d/10", key_count);
    CHECK(touch_count == 10, "ten physical taps arrive: %d/10", touch_count);
    CHECK(ink_test_index_count() > before && !ready(), "indexing progresses throughout input burst");
    hw(HW_KEY_DOWN, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(ink_audio_capture_busy(), 1500), "physical AI key opens microphone during indexing");
    delay_ms(800);
    hw(HW_KEY_UP, HAL_KEY_AI, 0);
    ink_probe_value("test.order.keys", key_count);
    ink_probe_value("test.order.touches", touch_count);
    ink_probe_value("test.order.mic_lost_ms", sim_metric_value("audio.mic.lost_ms"));
    CHECK(sim_metric_value("audio.mic.lost_ms") == 0, "no microphone samples lost: %.0f ms", sim_metric_value("audio.mic.lost_ms"));
    CHECK(WAIT_FOR(ready(), 30000), "index completes after interaction");
}

static const ink_db_def_t BGS[] = {
    {.name = "SYNTH.bg0", .scope = INK_DB_DEVICE}, {.name = "SYNTH.bg1", .scope = INK_DB_DEVICE},
    {.name = "SYNTH.bg2", .scope = INK_DB_DEVICE}, {.name = "SYNTH.bg3", .scope = INK_DB_DEVICE}};
static const ink_db_def_t SYNC = {.name = "SYNTH.sync", .scope = INK_DB_DEVICE};
static void priority(void) {
    CHECK(WAIT_FOR(ready() && !ink_storage_pending(), 10000), "boot writes settle");
    ink_db_t *bg[4], *sync = ink_db_open(&SYNC, NULL);
    for (int i = 0; i < 4; i++) bg[i] = ink_db_open(&BGS[i], NULL);
    ink_storage_hold_writes(true);
    ink_db_write_t w = {.data = "SYNTH", .len = 5, .flags = INK_DB_LAZY};
    uint8_t *payload = osal_malloc(8192, OSAL_MEM_EXTERNAL);
    CHECK(payload != NULL, "synthetic backlog buffer");
    if (!payload) return;
    memset(payload, 'X', 8192);
    ink_db_write_t large = {.data = payload, .len = 8192, .flags = INK_DB_LAZY};
    for (int i = 0; i < 4; i++) {
        int accepted = 0;
        for (int k = 0; bg[i] && k < 8; k++) accepted += !ink_db_put(bg[i], &large, NULL);
        CHECK(accepted == 8, "queue 64 KiB background batch %d", i);
        ink_db_flush_background(bg[i]);
    }
    osal_free(payload);
    CHECK(sync && !ink_db_put(sync, &w, NULL), "queue synchronous batch after background work");
    delay_ms(100);
    CHECK(ink_storage_pending() == 4, "four real batches queued behind the audio hold");
    uint64_t start = osal_now_us();
    ink_storage_hold_writes(false);
    CHECK(!ink_db_wait(sync, ink_db_last_ver(sync), 10000000), "wait returns only after persistence");
    ink_probe_value("test.order.sync_wait_ms", (double)(osal_now_us() - start) / 1000.0);
    int earlier = 0;
    for (int i = 0; i < 4; i++) earlier += ink_db_durable_ver(bg[i]) > 0;
    ink_probe_value("test.order.background_before_sync", earlier);
    CHECK(!earlier, "synchronous batch precedes four older background batches: %d earlier", earlier);
    CHECK(ink_storage_size("/user/sys/SYNTH.sync.0.log") == 41, "success is backed by the flash log");
    CHECK(WAIT_FOR(!ink_storage_pending(), 10000), "background work eventually persists");
    ink_db_close(sync);
    for (int i = 0; i < 4; i++) ink_db_close(bg[i]);
}

static void quiet(void) {
    test_start_home();
    CHECK(WAIT_FOR(ready() && !ink_storage_pending(), 10000), "boot writes settle");
    ink_db_t *db = ink_db_open(&SYNC, NULL);
    ink_db_write_t w = {.data = "SYNTH", .len = 5, .flags = INK_DB_LAZY};
    ink_power_activity(osal_now_us());
    CHECK(!ink_db_put(db, &w, NULL), "background batch just after activity");
    ink_db_flush_background(db);
    delay_ms(400);
    CHECK(!ink_db_durable_ver(db), "background does not start within 500 ms of input");
    CHECK(WAIT_FOR(ink_db_durable_ver(db) == ink_db_last_ver(db), 3000), "background resumes after input quiets");
    hw(HW_KEY_DOWN, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(ink_audio_capture_busy(), 1500), "AI key starts capture before queuing background write");
    CHECK(!ink_db_put(db, &w, NULL), "background batch during capture");
    uint32_t target = ink_db_last_ver(db);
    ink_db_flush_background(db);
    delay_ms(1000);
    CHECK(ink_db_durable_ver(db) < target, "background never starts during capture");
    hw(HW_KEY_UP, HAL_KEY_AI, 0);
    CHECK(WAIT_FOR(!ink_audio_capture_busy(), 3000), "capture ends");
    delay_ms(400);
    CHECK(ink_db_durable_ver(db) < target, "background also waits after capture");
    CHECK(!ink_db_wait(db, target, 5000000), "explicit waiter bypasses background quiet window");
    CHECK(sim_metric_value("audio.mic.lost_ms") == 0, "quiet-window capture loses no samples");
    ink_db_close(db);
}

static const ink_db_def_t PRESSURE = {.name = "SYNTH.pressure", .scope = INK_DB_DEVICE, .max_records = 2048, .max_bytes = 512 * 1024};
static void pressure(void) {
    CHECK(WAIT_FOR(ready() && !ink_storage_pending(), 10000), "boot writes settle");
    ink_db_t *db = ink_db_open(&PRESSURE, NULL);
    uint8_t data[216]; memset(data, 'X', sizeof data);
    ink_db_write_t w = {.data = data, .len = sizeof data, .flags = INK_DB_LAZY};
    ink_storage_hold_writes(true);
    int accepted = 0;
    for (int i = 0; i < 1041; i++) if (!ink_db_put(db, &w, NULL)) accepted++;
    size_t bytes = ink_test_pending_bytes(db), limit = ink_test_pending_limit();
    ink_probe_value("test.order.pressure_accepted", accepted);
    ink_probe_value("test.order.pending_bytes", (double)bytes);
    CHECK(accepted == 1041, "1024 index-sized records and 17 more survive pending limit: %d", accepted);
    CHECK(bytes <= limit && limit == 256 * 1024, "pending cap remains 256 KiB (%zu/%zu)", bytes, limit);
    ink_storage_hold_writes(false);
    CHECK(!ink_db_wait(db, ink_db_last_ver(db), 10000000), "pressure batches become durable");
    CHECK(ink_db_count(db) == 1041, "no record disappears at batching boundary");
    ink_db_close(db);
}

static void options(void *arg) {
    (void)arg;
    const char *s = getenv("INK_ORDER_MODE");
    snprintf(mode, sizeof mode, "%.23s", s ? s : "index");
}
static void run(void *arg) {
    (void)arg;
    if (!strcmp(mode, "priority")) priority();
    else if (!strcmp(mode, "quiet")) quiet();
    else if (!strcmp(mode, "pressure")) pressure();
    else if (!strcmp(mode, "input")) interactions();
    else {
        CHECK(WAIT_FOR(ready(), 30000), "automatic whole-card scan finishes");
        CHECK(ink_test_index_count() == (!strcmp(mode, "limit") ? 1024u : 1000u), "all synthetic files indexed");
        CHECK(ink_test_index_writes() <= 3, "whole-card index uses one append and completion metadata: %u", ink_test_index_writes());
    }
    ink_probe_value("test.order.index_writes", ink_test_index_writes());
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 15000), "all accepted writes drain");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    vtk_host_call(options, NULL);
    osal_task_cfg_t cfg = {.name = "storage-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task starts");
}
