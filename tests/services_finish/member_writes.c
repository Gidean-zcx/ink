#include "product.h"

static int results;
static char status[24];
static const ink_db_def_t PRIVATE = {.name = "synthetic.private", .scope = INK_DB_MEMBER, .schema = 1};

static void result(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    ink_args_get_str(args, "status", status, sizeof status);
    results++;
}

static bool decide(const char *cap, const char *args) {
    if (!BY_HAND(cap, args) || g_rc.status != INK_RC_PENDING_CONFIRMATION) return false;
    unsigned token = 0;
    if (sscanf(g_rc.object, "confirm:%u", &token) != 1) return false;
    int before = results;
    return ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0 && WAIT_FOR(results > before, 5000);
}

static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_DIRECT) == 0, "listener");
    CHECK(decide("member.add", "{\"id\":\"oldadult\",\"role\":\"adult\"}") && !strcmp(status, "done"), "add adult");
    uint32_t old = ink_member_exists("oldadult");
    CHECK(old && ink_member_io_begin("oldadult") && ink_member_io_begin("oldadult"), "register two writers");
    CHECK(decide("member.remove", "{\"id\":\"oldadult\"}") && !strcmp(status, "failed"), "registered writer blocks removal");
    CHECK(ink_member_exists("oldadult") == 0, "quarantined incarnation");
    CHECK(!ink_member_io_begin("oldadult"), "no new writer after quarantine");
    CHECK(decide("member.add", "{\"id\":\"oldadult\",\"role\":\"child\"}") && !strcmp(status, "failed"), "busy id cannot be reused");
    ink_member_io_end("oldadult");
    CHECK(decide("member.remove", "{\"id\":\"oldadult\"}") && !strcmp(status, "failed"), "second writer still holds id");
    ink_member_io_end("oldadult");
    CHECK(decide("member.remove", "{\"id\":\"oldadult\"}") && !strcmp(status, "done"), "retry after writers finish");
    CHECK(!ink_member_io_begin("oldadult"), "absent writer rejected");
    CHECK(ink_db_open(&PRIVATE, "oldadult") == NULL, "absent collection refused");
    CHECK(ink_storage_write_atomic("/user/m/oldadult/synthetic.raw", "SYNTH", 5) != 0, "absent raw write refused");
    CHECK(decide("member.add", "{\"id\":\"oldadult\",\"role\":\"child\"}") && !strcmp(status, "done"), "reuse after cleanup");
    uint32_t fresh = ink_member_exists("oldadult");
    CHECK(fresh && fresh != old && !ink_member_io_begin("oldadult"), "old incarnation cannot register");
    CHECK(ink_db_open_member(&PRIVATE, "oldadult") == NULL, "old collection opener refused");
    CHECK(ink_storage_write_member_atomic("/user/m/oldadult/synthetic.raw", "SYNTH", 5) != 0 &&
          ink_storage_remove_member("/user/m/oldadult/synthetic.raw", NULL, NULL) != 0, "old write and remove refused");
    CHECK(ink_storage_write_member_atomic("/user/sys/synthetic.raw", "SYNTH", 5) != 0 &&
          ink_storage_write_member_atomic("/user/m/oldadult/synthetic.raw", "SYNTH", 5) != 0, "bound API rejects device path and zero incarnation");
    ink_db_t *db = ink_db_open_member(&PRIVATE, "oldadult");
    CHECK(db != NULL, "live reference admitted");
    CHECK(decide("member.remove", "{\"id\":\"oldadult\"}") && !strcmp(status, "failed"), "collection reference blocks cleanup");
    ink_db_close(db);
    CHECK(decide("member.remove", "{\"id\":\"oldadult\"}") && !strcmp(status, "done"), "reference released without writer leak");
    CHECK(decide("member.add", "{\"id\":\"oldadult\",\"role\":\"child\"}") && !strcmp(status, "done"), "new child for queue control");
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 4000), "start with an empty queue");
    ink_storage_hold_writes(true);
    for (int i = 0; i < 8; i++)
        CHECK(ink_storage_write_borrow("/user/m/oldadult/synthetic.raw", "SYNTH", 5, NULL, NULL) == 0, "queue slot %d", i);
    CHECK(ink_storage_append("/user/m/oldadult/synthetic.raw", "SYNTH", 5, NULL, NULL) != 0 &&
          ink_storage_write_atomic("/user/m/oldadult/synthetic.other", "SYNTH", 5) != 0 &&
          ink_storage_remove("/user/m/oldadult/synthetic.raw", NULL, NULL) != 0, "full queue returns all registrations");
    ink_storage_hold_writes(false);
    CHECK(WAIT_FOR(!ink_storage_pending(), 4000), "accepted jobs finish");
    CHECK(decide("member.remove", "{\"id\":\"oldadult\"}") && !strcmp(status, "done"), "queue failures did not leak writers");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "member-writes", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
