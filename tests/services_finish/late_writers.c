#include "product.h"
#include "ink/sys/appreg.h"
#include "ink/sys/notify.h"

void ink_test_late_app_save(void);
void ink_test_late_media_save(bool card);
void ink_test_late_activity_save(void);
static const char *writer;
static bool held, release_writer, writer_done, armed;
static ink_exec_job_t writer_job;
static int results;
static char status[24];

void ink_test_late_gate(const char *kind) {
    if (!armed || strcmp(writer, kind)) return;
    armed = false;
    held = true;
    while (!release_writer) osal_task_delay_us(10000);
}

static void result(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    ink_args_get_str(a, "status", status, sizeof status);
    results++;
}

static bool confirmed(const char *cap, const char *args) {
    if (!BY_HAND(cap, args) || g_rc.status != INK_RC_PENDING_CONFIRMATION) return false;
    unsigned token = 0;
    if (sscanf(g_rc.object, "confirm:%u", &token) != 1) return false;
    int before = results;
    return ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0 && WAIT_FOR(results > before, 5000) && !strcmp(status, "done");
}

static void empty_page(lv_obj_t *scr, void *arg) { (void)scr; (void)arg; }
static int app_save(void *buf, size_t cap) {
    return !strcmp(writer, "app_remove") ? 0 : snprintf(buf, cap, "SYNTH_OLD");
}
static const ink_activity_t PAGE = {.name = "synthetic", .app = "synthetic", .create = empty_page};
static const ink_app_ops_t OPS = {.save_state = app_save};
static const ink_app_def_t APP = {.id = "synthetic", .title = "测试", .entry = &PAGE, .ops = &OPS, .child_ok = true};

static void write_late(void *arg) {
    (void)arg;
    if (!strncmp(writer, "app_", 4)) ink_test_late_app_save();
    else if (!strncmp(writer, "media_", 6)) ink_test_late_media_save(!strcmp(writer, "media_card"));
    else if (!strcmp(writer, "activity")) ink_test_late_activity_save();
    else if (!strcmp(writer, "notify_prefs")) ink_notify_channel_set("download", false);
    else {
        ink_notify_t n = {.title = "SYNTH_OLD", .member = "oldadult", .prio = INK_NOTIFY_NORMAL};
        ink_notify_post(&n, NULL);
    }
    writer_done = true;
}

static void noop(void) {}
static const ink_db_def_t MEDIA = {.name = "media.positions", .scope = INK_DB_MEMBER, .schema = 1, .json = true};
static const ink_db_def_t PREFS = {.name = "notify.prefs", .scope = INK_DB_MEMBER, .schema = 1, .json = true};

static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    writer = getenv("INK_REVIEW_WRITER");
    bool control = getenv("INK_REVIEW_CONTROL") != NULL;
    bool reuse = getenv("INK_REVIEW_NO_REUSE") == NULL;
    CHECK(ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_DIRECT) == 0, "listener");
    CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"adult\"}"), "add adult");
    CHECK(ink_member_switch("oldadult") == 0, "enter adult");
    delay_ms(300);
    bool app = !strncmp(writer, "app_", 4);
    if (app) {
        CHECK(ink_app_register(&APP) == 0, "test app");
        CHECK(BY_HAND("app.open", "{\"id\":\"synthetic\"}") && g_rc.status == INK_RC_DONE, "open real app lifecycle");
        CHECK(ink_storage_write_atomic("/user/m/oldadult/synthetic.state", "SYNTH_SEED", 10) == 0 &&
              WAIT_FOR(!ink_storage_pending(), 4000), "seed app state");
    }
    armed = true;
    if (app) CHECK(ink_exec_post(INK_EXEC_UI, &writer_job, write_late, NULL) == 0, "post app save");
    else {
        osal_task_cfg_t cfg = {.name = "late-writer", .fn = write_late, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
            .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
        CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "writer task");
    }
    CHECK(WAIT_FOR(held, 1000), "pause after validation before admission");
    CHECK(ink_member_switch("owner") == 0, "other adult takes device");
    if (!control) {
        CHECK(confirmed("member.remove", "{\"id\":\"oldadult\"}"), "remove before admission: %s", status);
        if (reuse) CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"child\"}"), "reuse as child");
        if (reuse && !strcmp(writer, "app_remove"))
            CHECK(ink_storage_write_atomic("/user/m/oldadult/synthetic.state", "SYNTH_NEW", 9) == 0, "new child's state");
    }
    release_writer = true;
    CHECK(WAIT_FOR(writer_done, 2000), "writer returned");
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 4000), "writes settled");
    char raw[512] = {0};
    if (app) {
        int n = ink_storage_read("/user/m/oldadult/synthetic.state", raw, sizeof raw - 1);
        if (!strcmp(writer, "app_remove")) CHECK(reuse && !control ? n == 9 && !strcmp(raw, "SYNTH_NEW") : n < 0,
            "stale remove preserves new child state: %s", raw);
        else CHECK(control ? n == 9 && !strcmp(raw, "SYNTH_OLD") : n < 0, "app state: %s", raw);
    } else if (!strcmp(writer, "media_card")) {
        ink_fs_file_t *f = NULL;
        int r = ink_fs_open("/cache/sys/media-pos", INK_FS_READ, &f);
        CHECK(control ? r == 0 : r != 0, "media card cache cannot return");
        if (f) ink_fs_close(f);
    } else if (control || reuse) {
        CHECK(ink_member_switch("oldadult") == 0, "enter saved member");
        on_ui(noop);
        delay_ms(300);
        if (!strcmp(writer, "media_flash") || !strcmp(writer, "notify_prefs")) {
            ink_db_t *db = ink_db_open(!strcmp(writer, "notify_prefs") ? &PREFS : &MEDIA, "oldadult");
            CHECK(db && ink_db_count(db) == (control ? 1 : 0), "saved collection contents");
            ink_db_close(db);
        } else {
            CHECK(BY_HAND(!strcmp(writer, "notify") ? "notify.list" : "activity.list", NULL) &&
                  (strstr(g_result, "SYNTH_OLD") != NULL) == control, "old record presence: %s", g_result);
        }
    } else {
        CHECK(ink_db_open(&MEDIA, "oldadult") == NULL, "absent member cannot reopen collection");
    }
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "late-writes", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
