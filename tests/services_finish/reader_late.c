#include "product.h"
#include "ink/sys/crc.h"

void ink_test_reader_save(void);
static bool ui_held, release_ui;
static ink_exec_job_t held_ui_job;
static int results;
static char result_status[24];

static void result(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    ink_args_get_str(args, "status", result_status, sizeof result_status);
    results++;
}

static bool confirmed(const char *cap, const char *args) {
    if (!BY_HAND(cap, args) || g_rc.status != INK_RC_PENDING_CONFIRMATION) return false;
    unsigned token = 0;
    if (sscanf(g_rc.object, "confirm:%u", &token) != 1) return false;
    int before = results;
    return ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0 && WAIT_FOR(results > before, 5000) && !strcmp(result_status, "done");
}

static void hold_ui(void *ctx) {
    (void)ctx;
    ui_held = true;
    while (!release_ui) osal_task_delay_us(10000);
    if (getenv("INK_REVIEW_SAVE_TIMER")) ink_test_reader_save();
}
static void noop(void) {}

static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    bool control = getenv("INK_REVIEW_CONTROL") != NULL;
    bool reuse = getenv("INK_REVIEW_NO_REUSE") == NULL;
    CHECK(ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_DIRECT) == 0, "listener");
    CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"adult\"}"), "add adult");
    CHECK(ink_member_switch("oldadult") == 0, "enter adult");
    delay_ms(300);
    uint32_t generation = ink_member_generation("oldadult");
    CHECK(AI("reader.open_book", "{\"title\":\"测试长书\"}") && g_rc.status == INK_RC_DONE, "open actual reader");
    CHECK(AI("reader.goto_page", "{\"page\":3}") && g_rc.status == INK_RC_DONE, "adult advances to page 3");
    char path[96], raw[64] = {0};
    snprintf(path, sizeof path, "/user/m/oldadult/reader.%08x.pos", (unsigned)ink_crc32(0, "测试长书", strlen("测试长书")));
    CHECK(ink_exec_post(INK_EXEC_UI, &held_ui_job, hold_ui, NULL) == 0 && WAIT_FOR(ui_held, 1000), "delay UI cleanup");
    CHECK(ink_member_switch("owner") == 0, "other adult takes device");
    if (!control) {
        CHECK(confirmed("member.remove", "{\"id\":\"oldadult\"}"), "delete while page cleanup queued: %s", result_status);
        CHECK(ink_member_generation("oldadult") == 0, "removed incarnation unavailable");
        if (reuse) {
            CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"child\"}"), "reuse as child");
            CHECK(ink_member_generation("oldadult") != generation, "reused id has a new incarnation");
        }
        CHECK(ink_storage_read(path, raw, sizeof raw - 1) < 0, "old position absent before delayed save");
    }
    release_ui = true;
    on_ui(noop);
    CHECK(WAIT_FOR(!ink_storage_pending(), 4000), "late saves settled");
    int n = ink_storage_read(path, raw, sizeof raw - 1);
    if (control) CHECK(n > 0, "normal handoff preserves adult position");
    else CHECK(n < 0, "deleted adult position returned: %s", raw);
    if (control || reuse) {
        CHECK(ink_member_switch("oldadult") == 0, "enter member again");
        delay_ms(300);
        CHECK(AI("reader.open_book", "{\"title\":\"测试长书\"}") && g_rc.status == INK_RC_DONE, "reopen actual reader");
        CHECK(AI("reader.read_page", NULL) && g_rc.status == INK_RC_DONE && g_rc.revision == (control ? 3u : 1u),
              "restored page receipt %u", g_rc.revision);
    }
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "review3-reader", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
