/* Audit-only observations on the unchanged whole product. Synthetic data only. */
#include <stdio.h>
#include <string.h>
#include "ink/sys/svc.h"
#include "ink/sys/notify.h"
#include "ink/sys/file.h"
#define ink_main product_main
#include "../../../../system/boot/main.c"
#undef ink_main
#include "../../../../tests/integration/whole_product.h"

static osal_sem_t *bg_in, *bg_go;
static ink_exec_job_t bg_job;
static void hold_background(void *ctx) {
    (void)ctx;
    osal_sem_give(bg_in);
    osal_sem_take(bg_go, OSAL_WAIT_FOREVER);
}
static void hold(void) {
    CHECK(ink_exec_post(INK_EXEC_BG, &bg_job, hold_background, NULL) == 0, "hold posted");
    CHECK(osal_sem_take(bg_in, 2000000) == OSAL_OK, "background held");
}
static void release(void) { osal_sem_give(bg_go); delay_ms(500); }
static void owner(void) { ink_member_test_reset(); delay_ms(500); }

static void stale_inbox(void) {
    uint32_t id = 0;
    ink_notify_t n = {.member = "owner", .title = "AUDIT_OWNER_INBOX", .body = "owner-only synthetic text", .prio = INK_NOTIFY_LOW};
    CHECK(ink_notify_post(&n, &id) == 0 && id, "owner notification seeded");
    hold();
    CHECK(ink_member_switch("kid") == 0, "switch to child");
    CHECK(AI("notify.list", NULL) && g_rc.status == INK_RC_DONE, "child reads through policy gate");
    bool leaked = strstr(g_result, "AUDIT_OWNER_INBOX") != NULL;
    printf("OBS stale_inbox current=%s status=%s owner_title_visible=%d\n", ink_member_current()->id, ink_rc_name(g_rc.status), leaked);
    CHECK(leaked, "reproduce stale inbox disclosure");
    char args[40];
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id);
    CHECK(AI("notify.dismiss", args) && g_rc.status == INK_RC_DONE, "child dismisses old notification");
    release();
    owner();
    CHECK(AI("notify.list", NULL), "owner reads again");
    bool deleted = !strstr(g_result, "AUDIT_OWNER_INBOX");
    printf("OBS stale_inbox owner_notification_deleted=%d\n", deleted);
    CHECK(deleted, "reproduce cross-member notification deletion");
}

static void queued_write(void) {
    hold();
    ink_call_opts_t o = {.src = INK_SRC_AI, .args_json = "{\"path\":\"/me/audit-queued.txt\",\"text\":\"owner-request\"}",
                         .reply_exec = INK_EXEC_DIRECT, .done = rc_done};
    g_done = 0;
    CHECK(ink_cap_call("file.write", NULL, &o) == 0 && !g_done, "owner AI write queued");
    CHECK(ink_member_switch("kid") == 0, "member changes before provider starts");
    release();
    CHECK(WAIT_FOR(g_done, 2000), "write receipt arrived");
    char value[64] = "";
    ink_fs_read("/me/audit-queued.txt", 0, value, sizeof value - 1);
    printf("OBS queued_write current=%s status=%s child_file_has_owner_request=%d\n", ink_member_current()->id, ink_rc_name(g_rc.status), !strcmp(value, "owner-request"));
    CHECK(g_rc.status == INK_RC_DONE && !strcmp(value, "owner-request"), "reproduce call rebound to child");
    owner();
}

static void delayed_device_event(void) {
    hold();
    ink_args_t a;
    ink_args_init(&a);
    ink_args_str(&a, "from", "AUDIT_SENDER");
    ink_args_str(&a, "text", "AUDIT_OWNER_MESSAGE");
    ink_args_str(&a, "member", "owner");
    ink_cap_emit("message.received", &a);
    CHECK(ink_member_switch("kid") == 0, "switch before notification subscriber runs");
    release();
    CHECK(AI("notify.list", NULL), "child reads inbox after background settles");
    bool leaked = strstr(g_result, "AUDIT_OWNER_MESSAGE") != NULL;
    printf("OBS delayed_device_event current=%s old_message_in_child_inbox=%d\n", ink_member_current()->id, leaked);
    CHECK(leaked, "reproduce local event subscriber losing owner");
    owner();
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    stale_inbox();
    queued_write();
    delayed_device_event();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    bg_in = osal_sem_create(0, 1);
    bg_go = osal_sem_create(0, 1);
    osal_task_cfg_t cfg = {.name = "audit-isolation", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("audit task");
}
