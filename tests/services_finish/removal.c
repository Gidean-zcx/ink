#include "product.h"
#include "ink/hal_power.h"
#include "ink/sys/layout.h"

int removal_fail_card;
static int fail_flash;
static hal_dir_t *removal_opendir(const char *path) {
    if (fail_flash && !strcmp(path, "/user/m/oldadult")) {
        fail_flash = 0;
        ink_probe_count("review.flash_io_failure");
        return NULL;
    }
    return hal_fs_opendir(path);
}
#define hal_fs_opendir removal_opendir
#ifndef INK_TEST_MEMBERS_SOURCE
#define INK_TEST_MEMBERS_SOURCE "../../system/policy/members.c"
#endif
#include INK_TEST_MEMBERS_SOURCE
#undef hal_fs_opendir

static const ink_db_def_t PRIVATE = {.name="review.private", .scope=INK_DB_MEMBER, .schema=1, .json=true, .ai_read=true};
static int results;
static char status[24];
static int pin_done, pin_result;
static void pin_told(void *ctx, int r, uint32_t wait_s) {
    (void)ctx; (void)wait_s;
    pin_result = r; pin_done++;
}
static void result(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    ink_args_get_str(args, "status", status, sizeof status);
    results++;
}
static bool confirmed(const char *cap, const char *args, const char *expected) {
    if (!BY_HAND(cap, args) || g_rc.status != INK_RC_PENDING_CONFIRMATION) return false;
    unsigned token = 0; sscanf(g_rc.object, "confirm:%u", &token);
    int before = results;
    if (ink_confirm_decide(token, true, INK_SRC_TOUCH) != 0 &&
        ink_confirm_accept_with_pin(token, "6767", pin_told, NULL) != 0) return false;
    return WAIT_FOR(results > before, 5000) && !strcmp(status, expected);
}
static void checkpoint(void) {
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 4000), "checkpoint is durable");
    ink_probe_count("review.removal_checkpoint");
    hal_power_reboot();
}
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_DIRECT) == 0, "confirmation listener");
    CHECK(ink_db_register(&PRIVATE) == 0, "private collection");
    const char *phase = getenv("INK_REVIEW_PHASE");
    if (!strcmp(phase, "prepare")) {
        CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"adult\"}", "done"), "add adult");
        CHECK(ink_member_switch("oldadult") == 0, "old adult uses device");
        bool pin = !strcmp(getenv("INK_REVIEW_FAILURE"), "pin");
        if (pin) {
            int before = pin_done;
            CHECK(ink_member_set_pin("oldadult", NULL, "6767", pin_told, NULL) == 0 &&
                  WAIT_FOR(pin_done > before, 2000) && pin_result == INK_PIN_OK, "old adult has the only PIN");
        }
        CHECK(ink_fs_write_atomic("/me/private.txt", "SYNTHETIC_OLD_ADULT_DATA", 24) == 0, "private card file");
        CHECK(ink_storage_write_atomic("/user/m/oldadult/app.state", "SYNTHETIC_FLASH", 15) == 0, "private flash file");
        ink_db_t *db = ink_db_open(&PRIVATE, "oldadult");
        ink_db_write_t w = {.data="{\"old\":true}", .len=12};
        CHECK(db && ink_db_put(db, &w, NULL) == 0 && ink_db_wait(db, ink_db_last_ver(db), 2000000) == 0, "private cached collection");
        bool cache = !strcmp(getenv("INK_REVIEW_FAILURE"), "cache");
        if (!cache && db) ink_db_close(db);
        CHECK(ink_member_switch("owner") == 0, "owner manages family");
        delay_ms(200);
        removal_fail_card = pin || !strcmp(getenv("INK_REVIEW_FAILURE"), "card");
        fail_flash = !strcmp(getenv("INK_REVIEW_FAILURE"), "flash");
        CHECK(confirmed("member.remove", "{\"id\":\"oldadult\"}", "failed"), "incomplete cleanup reports failure");
        CHECK(!removal_fail_card && !fail_flash, "injected failure was reached");
        CHECK(ink_member_switch("oldadult") != 0, "quarantined adult cannot enter");
        int before = pin_done;
        CHECK(ink_member_unlock("oldadult", "6767", pin_told, NULL) == 0 &&
              WAIT_FOR(pin_done > before, 2000) && pin_result == INK_PIN_NO_MEMBER, "PIN cannot reopen the quarantined identity");
        CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"child\"}", "failed"), "quarantined id cannot be reused");
        if (cache && db) ink_db_close(db);
        checkpoint();
    } else if (!strcmp(phase, "retry")) {
        CHECK(AI("member.list", NULL) && strstr(g_result, "\"removing\":true"), "quarantine survives reboot and is queryable");
        CHECK(ink_member_switch("oldadult") != 0, "reboot does not reopen the quarantined identity");
        CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"child\"}", "failed"), "id stays reserved after reboot");
        CHECK(confirmed("member.remove", "{\"id\":\"oldadult\"}", "done"), "retry finishes all cleanup");
        CHECK(ink_storage_size("/user/m/oldadult/app.state") < 0, "old flash file is gone");
        CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"child\"}", "done"), "reuse only after cleanup");
        checkpoint();
    } else {
        CHECK(ink_member_switch("oldadult") == 0 && ink_member_current()->role == INK_ROLE_CHILD, "new child persists");
        CHECK(AI("file.read", "{\"path\":\"/me/private.txt\"}") && !strstr(g_result, "SYNTHETIC_OLD_ADULT_DATA"), "new child has no old private file");
        ink_db_t *db = ink_db_open(&PRIVATE, "oldadult");
        CHECK(db && ink_db_count(db) == 0, "new child has no old cached or durable records");
        if (db) ink_db_close(db);
        ink_probe_count("test.finished");
    }
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="removal-regression", .fn=run, .stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
